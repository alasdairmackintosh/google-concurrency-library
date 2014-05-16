// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GCL_PIPELINE_
#define GCL_PIPELINE_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <exception>
#include <stdexcept>

#include "functional.h"

#include "atomic.h"
#include "barrier.h"
#include "buffer_queue.h"
#include "countdown_latch.h"
#include "debug.h"
#include "notifying_barrier.h"
#include "queue_base.h"
#include "simple_thread_pool.h"

using std::string;
using std::vector;

namespace gcl {

namespace pipeline {

  // BEGIN UTILITIES
// TODO(aberkan): Move some of this out of this file
// TODO(aberkan): Move excecution strategy to template parameter
// TODO(aberkan): Reduce Cloning
// TODO(alasdair): Remove debug methods (principally names for instances), or
// make them formal and document them. Currently debugging pipelines is tricky,
// so we are keeping these for now.

  // BEGIN DEBUGGING VARIABLES
// Static variables used for assigning names and IDs to various
// pipeline elements. These are very messy.

std::atomic<int> plan_id_count_(0);
std::atomic<int> filter_thread_point_count(0);
std::atomic<int> filter_count(0);
std::atomic<int> segment_count(0);

static mutex qcount_lock_;
static int qcount_ = 0;
static vector<string> qnames_;
const char* get_q_name(const char* prefix) {
  unique_lock<mutex> l(qcount_lock_);
  std::stringstream new_name;
  new_name << prefix << "-" << ++qcount_;
  qnames_.push_back(new_name.str());
  return new_name.str().c_str();
}
  // END DEBUGGING VARIABLES

class terminated { };

template <typename IN, typename OUT>
class segment;
template <typename IN, typename OUT>
class __segment_base;

class __instance {
 public:
  __instance(simple_thread_pool* pool,
           __segment_base<terminated, terminated>* p);
  ~__instance();

  bool is_done() {
    return done_;
  }
  void wait() {
    end_.wait();
    thread_end_->arrive_and_wait();
    assert(is_done());
  }

  void thread_start() {
    start_.wait();
  }
  void thread_done() {
    thread_end_->arrive_and_wait();
  }
  void execute(function<void ()> func) {
    num_threads_++;
    // TODO(aberkan): handle errors
    pool_->try_get_unused_thread()->execute(func);
  }

  size_t all_threads_done() {
    // This method is invoked after all threads have called
    // count_down_and_wait(), but not before they have all exited. To ensure
    // that a caller cannot return from wait() until all threads have exited
    // count_down_and_wait(), we reset the barrier, and re-use it for a final
    // test in __instance::wait().
    bool do_count_down = done_ ? false : true;
    done_ = true;
    if (do_count_down) {
      end_.count_down();
    }
    return 1;
  }

 private:
  countdown_latch start_;
  countdown_latch end_;
  int num_threads_;
  bool done_;
  notifying_barrier* thread_end_;
  simple_thread_pool* pool_;
  queue_object<buffer_queue<terminated> > dummy_queue_;

  __segment_base<terminated, terminated>* plan_;

  friend class segment<terminated, terminated>;
};

class execution {
 public:
  execution(__instance* inst) : inst_(inst) {}
  execution(const execution& exec); // undefined
  ~execution() {
    delete inst_;
  }
  bool is_done() {
    return inst_->is_done();
  }
  void wait() {
    inst_->wait();
  }

private:

  // TODO(aberkan): should be shared_ptr
  __instance* inst_;
};

typedef segment<terminated, terminated> plan;

void nothing() {};

template<typename T>
terminated do_consume(std::function<void (T)> f, T t) {
  f(t);
  return terminated();
}

  // START WORKER THREADS

template<typename IN,
         typename OUT>
void run_simple_function(queue_front<IN> in_queue,
                         queue_back<OUT> out_queue,
                         function<OUT(IN)> func,
                         __instance* inst) {
  inst->thread_start();
  while (1) {
    IN in;
    queue_op_status status = in_queue.wait_pop(in);
    if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
      break;  // Queue closed
    }
    out_queue.push(func(in));
  }
  out_queue.close();
  inst->thread_done();
}

template<typename IN,
         typename OUT>
void run_multi_out_function(queue_front<IN> in_queue,
                            queue_back<OUT> out_queue,
                            function<void (IN, queue_back<OUT>)> func,
                            __instance* inst) {
  inst->thread_start();
  while (1) {
    IN in;
    queue_op_status status = in_queue.wait_pop(in);
    if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
      break;  // Queue closed
    }
    func(in, out_queue);
  }
  out_queue.close();  // TODO(aberkan): something for parallel
  inst->thread_done();
}

template<typename IN,
         typename OUT>
void run_multi_in_function(queue_front<IN> in_queue,
                           queue_back<OUT> out_queue,
                           function<OUT(queue_front<IN>)> func,
                           __instance* inst) {
  inst->thread_start();
  while (!in_queue.is_closed()) {
    out_queue.push(func(in_queue));
  }
  out_queue.close();  // TODO(aberkan): something for parallel
  inst->thread_done();
}

template<typename IN,
         typename OUT>
void run_full_function(queue_front<IN> in_queue,
                       queue_back<OUT> out_queue,
                       function<void(queue_front<IN>, queue_back<OUT>)> func,
                       __instance* inst) {
  inst->thread_start();
  while (!in_queue.is_closed()) {
    func(in_queue, out_queue);
  }
  out_queue.close();  // TODO(aberkan): something for parallel
  inst->thread_done();
}

template<typename IN>
void run_consumer(queue_front<IN> in_queue,
                  function<void(IN)> func,
                  __instance* inst) {
  inst->thread_start();
  while (1) {
    IN in;
    queue_op_status status = in_queue.wait_pop(in);
    if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
      break;  // Queue closed
    }
    func(in);
  }
  inst->thread_done();
}

template<typename IN>
void run_multi_in_consumer(queue_front<IN> in_queue,
                           function<void(queue_front<IN>)> func,
                           __instance* inst) {
  inst->thread_start();
  while (!in_queue.is_closed()) {
    func(in_queue);
  }
  inst->thread_done();
}


template<typename OUT>
void run_producer(function<OUT (void)> f,
                  queue_back<OUT> out_queue,
                  __instance* inst) {
  inst->thread_start();
  out_queue.push(f());
  out_queue.close();  // TODO(aberkan): something for parallel
  inst->thread_done();
}

template<typename OUT>
void run_multi_out_producer(function<void (queue_back<OUT>)> f,
                            queue_back<OUT> out_queue,
                            __instance* inst) {
  inst->thread_start();
  f(out_queue);
  out_queue.close();  // TODO(aberkan): something for parallel
  inst->thread_done();
}

template<typename T>
void run_queue_internal(queue_front<T> ft, queue_back<T> bk) {
  while (1) {
    T t;
    queue_op_status status = ft.wait_pop(t);
    if (status != CXX11_ENUM_QUAL(queue_op_status)success) {
      return;  // Queue closed
    }
    bk.push(t);
  }
}

template<typename T>
void run_queue(queue_front<T> ft, queue_back<T> bk, __instance* inst) {
  inst->thread_start();
  run_queue_internal(ft, bk);
  bk.close();
  inst->thread_done();
}

// END WORKER THREADS

template<typename IN,
         typename OUT>
class __segment_base {
 public:
  virtual ~__segment_base() {};
  virtual void run(__instance* inst, queue_back<OUT> out_queue) = 0;
  virtual __segment_base<IN, OUT>* clone() = 0;
  virtual queue_back<IN> get_back() = 0;
  virtual bool can_merge_on_front() { return false; }
  virtual bool can_merge_on_back() { return false; }
  virtual void merge_on_front(queue_front<IN> in_queue) {
    throw;
  }
  virtual queue_front<OUT> merge_back() {
    throw;
  }
 protected:
  __segment_base() {};
};

template<typename IN,
         typename MID,
         typename OUT>
class __segment_chain : public __segment_base<IN, OUT> {
 public:
  __segment_chain(__segment_base<IN, MID>* first,
                  __segment_base<MID, OUT>* second) :
      first_(first), second_(second) {
        if (first_->can_merge_on_back() && second_->can_merge_on_front()) {
          second_->merge_on_front(first_->merge_back());
        }
      }

  __segment_chain<IN, MID, OUT>* clone() {
    return new __segment_chain(first_->clone(), second_->clone());
  }

  virtual ~__segment_chain() {
    delete first_;
    delete second_;
  }

  virtual bool can_merge_on_front() { return first_->can_merge_on_front();}
  virtual void merge_on_front(queue_front<IN> ft) {
    first_->merge_on_front(ft);
  }
  virtual bool can_merge_on_back() { return second_->can_merge_on_back(); }
  virtual queue_front<OUT> merge_back() {
    return second_->merge_back();
  }


 private:
  virtual void run(__instance* inst, queue_back<OUT> out_queue) {
    first_->run(inst, second_->get_back());
    second_->run(inst, out_queue);
  }
  virtual queue_back<IN> get_back() { return first_->get_back(); }

  __segment_base<IN, MID>* first_;
  __segment_base<MID, OUT>* second_;
};

template<typename IN,
         typename OUT>
class __segment_function : public __segment_base<IN, OUT> {
 public:
  virtual ~__segment_function() {}

  __segment_function(function<OUT (IN)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_simple_function<IN, OUT>,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      f,
                      std::placeholders::_3)),
      has_merged_in_queue_(false),
      merged_in_queue_(NULL) {}

  __segment_function(function<OUT (queue_front<IN>)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_multi_in_function<IN, OUT>,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      f,
                      std::placeholders::_3)),
      has_merged_in_queue_(false),
      merged_in_queue_(NULL) {}

  __segment_function(function<void (IN, queue_back<OUT>)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_multi_out_function<IN, OUT>,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      f,
                      std::placeholders::_3)),
      has_merged_in_queue_(false),
      merged_in_queue_(NULL) {}

  __segment_function(function<void (queue_front<IN>, queue_back<OUT>)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_full_function<IN, OUT>,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      f,
                      std::placeholders::_3)),
      has_merged_in_queue_(false),
      merged_in_queue_(NULL) {}

  virtual bool can_merge_on_front() { return !has_merged_in_queue_;}
  virtual void merge_on_front(queue_front<IN> ft) {
    merged_in_queue_ = ft;
    has_merged_in_queue_ = true;
  }

 private:
  __segment_function(const __segment_function<IN, OUT>& f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(f.func_),
      has_merged_in_queue_(f.has_merged_in_queue_),
      merged_in_queue_(f.merged_in_queue_) {}

  virtual void run(__instance* inst, queue_back<OUT> out_queue) {
    // TODO(aberkan): Check for failures from both functions
    inst->execute(std::bind(
        func_,
        has_merged_in_queue_ ? merged_in_queue_ : queue_front<IN>(queue_),
        out_queue,
        inst));
  }
  virtual __segment_function<IN, OUT>* clone() {
    return new __segment_function<IN, OUT>(*this);
  }

  virtual queue_back<IN> get_back() {
    return (has_merged_in_queue_ ? NULL : queue_back<IN>(queue_));
  }

  queue_object<buffer_queue<IN> > queue_;
  function<void (queue_front<IN>, queue_back<OUT>, __instance*)> func_;

  bool has_merged_in_queue_;
  queue_front<IN> merged_in_queue_;
};

template<typename OUT>
class __segment_producer : public __segment_base<terminated, OUT> {
 public:
  __segment_producer(function<OUT (void)> f) :
      func_(std::bind(run_producer<OUT>,
                      f,
                      std::placeholders::_1,
                      std::placeholders::_2)) {}

  __segment_producer(function<void (queue_back<OUT>)> f) :
      func_(std::bind(run_multi_out_producer<OUT>,
                      f,
                      std::placeholders::_1,
                      std::placeholders::_2)) {}

  virtual ~__segment_producer() {}

  virtual void run(__instance* inst, queue_back<OUT> out_queue) {
    inst->execute(std::bind(func_, out_queue, inst));
  }
  virtual queue_back<terminated> get_back() {
    // TODO(aberkan): If we want to combine plans, this would need to be
    // implemented.
    throw;  // Unimplemented
  }
  virtual __segment_producer<OUT>* clone() {
    return new __segment_producer<OUT>(*this);
  }

  // TODO(aberkan): should be private to __segment_parallel
  __segment_producer(function<void (queue_back<OUT>, __instance*)> func) :
      func_(func) {}
 private:
  __segment_producer(const __segment_producer<OUT>& f) :
      func_(f.func_) {}

  function<void (queue_back<OUT>, __instance*)> func_;
};

template<typename OUT>
class __segment_queue_producer : public __segment_base<terminated, OUT> {
 public:
  __segment_queue_producer(queue_front<OUT> ft) :
      ft_(ft),
      has_been_merged_(false) {}

  virtual ~__segment_queue_producer() {}

  virtual void run(__instance* inst, queue_back<OUT> out_queue) {
    if(!has_been_merged_) {
      inst->execute(std::bind(run_queue<OUT>, ft_, out_queue, inst));
    } else {
      // If this has been merged, the mergee will pull directly from our queue and
      // we have nothing to do.
      assert(!out_queue.has_queue());
    }
  }
  virtual queue_back<terminated> get_back() {
    // TODO(aberkan): If we want to combine plans, this would need to be
    // implemented.
    throw;  // Unimplemented
  }
  virtual __segment_queue_producer<OUT>* clone() {
    return new __segment_queue_producer<OUT>(*this);
  }

  virtual bool can_merge_on_back() { return !has_been_merged_; }
  virtual queue_front<OUT> merge_back() {
    queue_front<OUT> ret = ft_;
    ft_ = NULL;
    has_been_merged_ = true;
    return ret;
  }

 private:
  __segment_queue_producer(const __segment_queue_producer<OUT>& f) :
      ft_(f.ft_), has_been_merged_(f.has_been_merged_) {}

  queue_front<OUT> ft_;
  bool has_been_merged_;
};

template<typename IN>
class __segment_consumer : public __segment_base<IN, terminated> {
 public:
  __segment_consumer(function<void (IN)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_consumer<IN>,
                      std::placeholders::_1,
                      f,
                      std::placeholders::_2)) {}

  __segment_consumer(function<void (queue_front<IN>)> f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(std::bind(run_multi_in_consumer<IN>,
                      std::placeholders::_1,
                      f,
                      std::placeholders::_2)) {}

  virtual ~__segment_consumer() {}

 private:
  __segment_consumer(const __segment_consumer<IN>& f) :
      queue_(10),  // TODO(aberkan): remove limit
      func_(f.func_) {}

  virtual void run(__instance* inst, queue_back<terminated> out_queue) {
    // TODO(aberkan): Check for failures from both functions
    inst->execute(std::bind(func_, queue_front<IN>(queue_), inst));
  }
  virtual __segment_consumer<IN>* clone() {
    return new __segment_consumer<IN>(*this);
  }

  virtual queue_back<IN> get_back() { return queue_back<IN>(queue_); }
  queue_object<buffer_queue<IN> > queue_;

  function<void (queue_front<IN>, __instance*)> func_;
};

template<typename IN>
class __segment_queue_consumer : public __segment_base<IN, terminated> {
 public:
  __segment_queue_consumer(queue_back<IN> bk) :
      bk_(bk) {}
  virtual ~__segment_queue_consumer() {}

  virtual void run(__instance* inst, queue_back<terminated> out_queue) {
    // NO-OP
  }
  virtual queue_back<IN> get_back() { return bk_; }
  virtual __segment_queue_consumer<IN>* clone() {
    return new __segment_queue_consumer<IN>(bk_);
  }
 private:
  queue_back<IN> bk_;
};

  // Parallel
template<typename IN,
         typename OUT>
class __segment_parallel : public __segment_base<IN, OUT> {
 public:
  __segment_parallel(__segment_base<IN, OUT>* s, size_t n) :
      in_queue_(10), s_(s), bases_(n), out_queues_(n) {
    __segment_queue_producer<IN> p(in_queue_);
    for (size_t i = 0; i < n; ++i) {
      bases_[i] = new __segment_chain<terminated, IN, OUT>(p.clone() ,s->clone());
      out_queues_[i] = new queue_object<buffer_queue<OUT> >(10);
    }
  }
  virtual ~__segment_parallel() {
    while (!bases_.empty()) {
      delete bases_.back();
      bases_.pop_back();
      delete out_queues_.back();
      out_queues_.pop_back();
    }
    delete s_;
  }
  virtual void run(__instance* inst, queue_back<OUT> out_queue) {
    for (size_t i = 0; i < bases_.size(); ++i) {
      bases_[i]->run(inst, out_queues_[i]->back());
    }
    inst->execute(std::bind(&__segment_parallel::run_out_queues,
                            this, out_queue, inst));
  }

  virtual queue_back<IN> get_back() { return in_queue_.back(); }
  virtual __segment_parallel<IN, OUT>* clone() {
    return new __segment_parallel<IN, OUT>(s_->clone(), bases_.size());
  }

private:
  void run_out_queues(queue_back<OUT> out_queue, __instance* inst) {
    // TODO(aberkan):  This is terrible...  Ideally we shouldn't have this step
    // at all, but at least we should check queues in parallel.
    inst->thread_start();
    for (size_t i = 0; i < out_queues_.size(); ++i) {
      run_queue_internal<OUT>(out_queues_[i]->front(), out_queue);
    }
    out_queue.close();
    inst->thread_done();
  }

  queue_object<buffer_queue<IN> > in_queue_;
  __segment_base<IN, OUT>* s_;
  std::vector<__segment_base<terminated, OUT>*> bases_;
  std::vector<queue_object<buffer_queue<OUT> >* > out_queues_;
};

  // END UTILITIES

  // BEGIN CLASSES

template<typename IN,
         typename OUT>
class segment {
 public:
  ~segment() { delete base_; }

  execution run(simple_thread_pool* pool);

  segment(const segment<IN, OUT>& s) :
      base_(s.base_->clone()) {}

  //TODO make private
  // Takes ownership of base.
  segment(__segment_base<IN, OUT>* base) : base_(base) {}
  //TODO make private
  __segment_base<IN, OUT>* base_;

  typedef OUT out;
  typedef IN in;
};

  // END CLASSES

  // START EXECUTION IMPLEMENTATION

template<>
execution segment<terminated, terminated>::run(simple_thread_pool* pool) {
  __segment_base<terminated, terminated>* plan = base_->clone();
  return execution(new __instance(pool, plan));
}


__instance::__instance(simple_thread_pool* pool,
                   __segment_base<terminated, terminated>* p) :
    start_(1), end_(1), num_threads_(0),
    done_(false),
    thread_end_(NULL), pool_(pool),
    dummy_queue_(1),
    plan_(p) {
  plan_->run(this, queue_back<terminated>(dummy_queue_));
  // We can't create the barrier until all of the threads have started running
  // so we know num_threads_.
  function<size_t()> done_fn = std::bind(&__instance::all_threads_done,
                                         this);
  thread_end_ = new notifying_barrier(num_threads_, done_fn);
  start_.count_down();  // Start the threads
}

__instance::~__instance() {
  wait();
  delete plan_;
  delete thread_end_;
}
  // END EXECUTION IMPLEMENTATION

  // BEGIN CONSTRUCTORS

// Producers
template<typename OUT>
segment<terminated, OUT> from(
    std::function<void (queue_back<OUT>)> f) {
  return segment<terminated, OUT>(
      new __segment_producer<OUT>(f));
}
template<typename OUT>
segment<terminated, OUT> from(void f(queue_back<OUT>)) {
  return from(std::function<void (queue_back<OUT>)>(f));
}

template<typename OUT>
segment<terminated, OUT> from(queue_front<OUT> ft) {
  return segment<terminated, OUT>(
      new __segment_queue_producer<OUT>(ft));
}

template<typename Q>
segment<terminated, typename Q::value_type> from(Q& q) {
  return from(q.front());
}

// Middle segments
template<typename IN,
         typename OUT>
segment<IN, OUT> make(std::function<OUT (IN)> f) {
  return segment<IN, OUT>(new __segment_function<IN, OUT>(f));
}
template<typename IN,
         typename OUT>
segment<IN, OUT> make(OUT f(IN)) {
  return make(std::function<OUT (IN)>(f));
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make(std::function<OUT (queue_front<IN>)> f) {
  return segment<IN, OUT>(new __segment_function<IN, OUT>(f));
}
template<typename IN,
         typename OUT>
segment<IN, OUT> make(OUT f(queue_front<IN>)) {
  return make(std::function<OUT (queue_front<IN>)>(f));
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make(std::function<void (IN, queue_back<OUT>)> f) {
  return segment<IN, OUT>(new __segment_function<IN, OUT>(f));
}
template<typename IN,
         typename OUT>
segment<IN, OUT> make(void f(IN, queue_back<OUT>)) {
  return make(std::function<void (IN, queue_back<OUT>)>(f));
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make(std::function<void (queue_front<IN>,
                                          queue_back<OUT>)> f) {
  return segment<IN, OUT>(new __segment_function<IN, OUT>(f));
}
template<typename IN,
         typename OUT>
segment<IN, OUT> make(void f(queue_front<IN>, queue_back<OUT>)) {
  return make(std::function<void (queue_front<IN>, queue_back<OUT>)>(f));
}

// Consumers

template<typename IN>
segment<IN, terminated> to(std::function<void (IN)> consumer) {
  return segment<IN, terminated>(new __segment_consumer<IN>(consumer));
}

template<typename IN>
segment<IN, terminated> to(void consumer(IN)) {
  return to(std::function<void (IN)>(consumer));
}

template<typename IN>
segment<IN, terminated> to(queue_back<IN> bk) {
  return segment<IN, terminated>(
      new __segment_queue_consumer<IN>(bk));
}

template<typename Q>
segment<typename Q::value_type, terminated> to(Q& q) {
  return to(q.back());
}


// Parallel
template<typename IN,
         typename OUT>
segment<IN, OUT> parallel(segment<IN, OUT> p, int n) {
  return segment<IN, OUT>(new __segment_parallel<IN, OUT>(p.base_->clone(), n));
}

// END CONSTRUCTORS

// BEGIN PIPES
//
// 1:1 MID->OUT
template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           const segment<MID, OUT>& p2) {
  return segment<IN, OUT>(new __segment_chain<IN, MID, OUT>(p1.base_->clone(),
                                                            p2.base_->clone()));
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           std::function<OUT (MID)> f) {
  return p1 | make(f);
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           OUT f(MID)) {
  return p1 | make(f);
}

// 1:N MID->OUT
template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           std::function<void (MID, queue_back<OUT>)> f) {
  return p1 | make(f);
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           void f(MID, queue_back<OUT>)) {
  return p1 | make(f);
}

// 1:0 MID->OUT
template<typename IN,
         typename MID>
segment<IN, terminated> operator|(const segment<IN, MID>& p1,
                                  std::function<void (MID)> f) {
  return p1 | to(f);
}

template<typename IN,
         typename MID>
segment<IN, terminated> operator|(const segment<IN, MID>& p1,
                                  void f(MID)) {
  return p1 | to(f);
}


template<typename MID,
         typename OUT>
segment<MID, terminated> operator|(queue_front<MID> ft,
                                   const segment<MID, OUT>& p1) {
  return from(ft) | p1;
}

template<typename OUT,
         typename Q>
segment<terminated, OUT> operator|(Q& q,
                                  const segment<typename Q::value_type, OUT>& p1) {
  return from(q) | p1;
}

template<typename IN,
         typename MID>
segment<IN, terminated> operator|(const segment<IN, MID>& p1,
                                  queue_back<MID> bk) {
  return p1 | to(bk);
}

template<typename IN,
         typename Q>
segment<IN, terminated> operator|(const segment<IN, typename Q::value_type>& p1,
                                  Q& q) {
  return p1 | to(q);
}

// END PIPES

} // namespace pipeline

} // namespace gcl
#endif  // GCL_PIPELINE_
