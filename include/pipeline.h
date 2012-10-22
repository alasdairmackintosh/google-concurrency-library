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
#include "queue_base.h"
#include "simple_thread_pool.h"

using std::string;
using std::vector;

namespace gcl {

namespace pipeline {

// BEGIN UTILITIES
//TODO(aberkan): Common naming scheme
//TODO(aberkan): Better commenting
//TODO(aberkan): Move some of this out of this file
//TODO(aberkan): Simplify filter_base and segment stuff
//TODO(aberkan): Move excecution strategy to template parameter
//TODO(aberkan): Reduce Cloning
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

template <typename T>
terminated ignore(T t) { return terminated(); };

template<typename T>
terminated do_consume(std::function<void (T)> f, T t) {
  f(t);
  return terminated();
}
template<typename T>
void call_and_close(std::function< void (queue_front<T>)> f,
                    queue_front<T> qf) {
  f(qf);
  qf.close();
}

template <typename IN_TYPE,
          typename INTERMEDIATE,
          typename OUT_TYPE>
static OUT_TYPE chain(std::function<INTERMEDIATE (IN_TYPE in)> intermediate_fn,
                      std::function<OUT_TYPE (INTERMEDIATE in)> out_fn,
                      IN_TYPE in) {
  return out_fn(intermediate_fn(in));
}

template <typename IN,
          typename OUT>
class filter_base {
 public:
  filter_base() {
    set_id();
  }
  virtual ~filter_base() {
  }
  virtual OUT Apply(IN in) = 0;
  virtual bool Run(std::function<terminated (OUT)> r) = 0;
  virtual bool Run() = 0;
  virtual void Close() = 0;
  virtual filter_base<IN, OUT>* clone() const = 0;
  // TODO(alasdair): Debug methods - remove this or formalise it.
  virtual string get_id() const = 0;
 protected:
  void set_id() {
    id_ = ++filter_count;
  }
  int id_;
};

template <typename IN, typename OUT>
class segment;

typedef segment<terminated, terminated> plan;

class execution {
 public:
  execution(const plan& pp, simple_thread_pool* pool);

  ~execution();

  bool is_done() {
    return done_;
  }
  void wait() {
    end_.wait();
    thread_end_->count_down_and_wait();
    assert(is_done());
  }
  void cancel();

  void execute(filter_base<terminated, terminated> *f);

  void threads_done() {
    // This method is invoked after all threads have called
    // count_down_and_wait(), but not before they have all exited. To ensure
    // that a caller cannot return from wait() until all threads have exited
    // count_down_and_wait(), we reset the barrier, and re-use it for a final
    // test in execution::wait(). We clear the callback method for the
    // barrier so that this doesn't get called again.
    done_ = true;
    thread_end_->reset(static_cast<size_t>(1));
    thread_end_->reset(static_cast<std::function<void ()> >(NULL));
    end_.count_down();
  }

  plan* pp_;
  simple_thread_pool* pool_;
  countdown_latch start_;
  barrier* thread_end_;
  countdown_latch end_;
  int num_threads_;
  bool done_;
};


template <typename IN,
          typename MID,
          typename OUT>
class filter_chain : public filter_base<IN, OUT> {
 public:
  filter_chain(class filter_base<IN, MID> *first,
               class filter_base<MID, OUT> *second)
      : first_(first),
        second_(second) {assert(first && second);};
  virtual ~filter_chain() {
    delete first_;
    delete second_;
  };
  virtual OUT Apply(IN in) {
    return second_->Apply(first_->Apply(in));
  };
  virtual filter_base<IN, OUT>* clone() const {
    return new filter_chain<IN, MID, OUT>(first_->clone(), second_->clone());
  };
  //TODO(aberkan): Make calling run on non-runnable a compile time error.
  virtual bool Run(std::function<terminated (OUT)> r);
  virtual bool Run();
  virtual void Close() {
    first_->Close();
    second_->Close();
  }
  class filter_base<IN, MID>* first_;
  class filter_base<MID, OUT>* second_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  virtual string get_id() const {
    std::stringstream ss;
    ss << "filter_chain-" << this->id_ <<  "(" << first_->get_id()
       << " -> " + second_->get_id() + ")";
    return ss.str();
  }
};

template <typename IN,
          typename MID,
          typename OUT>
bool filter_chain<IN, MID, OUT>::Run(std::function<terminated (OUT)> r) {
  std::function<OUT (MID)> m = std::bind(&filter_base<MID, OUT>::Apply, second_,
                                         std::placeholders::_1);
  std::function<terminated (MID)> p
    = std::bind(chain<MID, OUT, terminated>, m, r, std::placeholders::_1);
  return first_->Run(p);
};

template <typename IN,
          typename MID,
          typename OUT>
bool filter_chain<IN, MID, OUT>::Run() {
  std::function<OUT (MID)> m = std::bind(&filter_base<MID, OUT>::Apply, second_,
                                         std::placeholders::_1);
  std::function<terminated (OUT)> r = ignore<OUT>;
  std::function<terminated (MID)> p =
    std::bind(chain<MID, OUT, terminated>, m, r, std::placeholders::_1);
  return first_->Run(p);
};

void nothing() {};

template <typename IN,
          typename OUT>
class filter_function : public filter_base<IN, OUT> {
 public:
  filter_function(std::function<OUT (IN)> f)
      : f_(f), close_(NULL) {
  };
  filter_function(std::function<OUT (IN)> f, std::function<void ()> close)
      : f_(f), close_(close) { };
  virtual ~filter_function() {
  };
  virtual OUT Apply(IN in) {
    return f_(in);
  }
  virtual bool Run(std::function<terminated (OUT)> r) {
    throw;
  }
  virtual bool Run() {
    throw;
  }
  virtual void Close() {
    close_();
  }
  virtual class filter_base<IN, OUT>* clone() const {
    return new filter_function<IN, OUT>(f_, close_);
  }
  std::function<OUT (IN)> f_;
  std::function<void ()> close_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  virtual string get_id() const {
    std::stringstream ss;
    ss << "filter_function-" << this->id_;
    return ss.str();
  }
};

template <typename OUT>
class filter_thread_point : public filter_base<terminated, OUT> {
 public:
  filter_thread_point(queue_back<OUT> qb) : qb_(qb), name_(qb.name()) {
    set_id();
  };
  ~filter_thread_point() { };
  virtual OUT Apply(terminated IN) {
    throw;
  }
  virtual bool Run(std::function<terminated (OUT)> r) {
    OUT out;
    // TODO(aberkan): What if this throws?
    queue_op_status status = qb_.wait_pop(out);
    if (status != CXX0X_ENUM_QUAL(queue_op_status)success) {
      return false;
    }
    r(out);
    return true;
  }
  virtual bool Run() {
    throw;
  }

  virtual void Close() { };

  virtual filter_base<terminated, OUT>* clone() const {
    return new filter_thread_point<OUT>(qb_);
  }
  queue_back<OUT> qb_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  virtual string get_id() const {
    std::stringstream ss;
    ss << "filter_thread_point " << count_ << " from " << name_;
    return ss.str();
  }

 private:
  // TODO(alasdair): Debug method - remove this or formalise it.
  void set_id() {
    count_ = ++filter_thread_point_count;
  }
  int count_;
  const char* name_;
};

class filter_producer_point : public filter_base<terminated, terminated> {
 public:
  filter_producer_point(std::function<void ()> f) : f_(f) { };
  virtual terminated Apply(terminated) {
    throw;
  }
  virtual bool Run(std::function<terminated (terminated)> r) {
    throw;
  }
  virtual bool Run() {
    f_();
    return false;
  }

  virtual void Close() { };
  virtual filter_base<terminated, terminated>* clone() const {
    return new filter_producer_point(f_);
  }
  std::function<void ()> f_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  virtual string get_id() const {
    std::stringstream ss;
    ss << "filter_producer_point-" << id_;
    return ss.str();
  }
};

template <typename OUT>
class run_point {
 public:
  run_point(filter_base<terminated, OUT> *f,
                   run_point<terminated> *next)
      : f_(f), next_(next) {
    set_id();
  };
  virtual ~run_point() {
    delete f_;
    delete next_;
  }
  void Run(execution* pex);
  run_point<OUT> *clone() const {
    return new run_point<OUT>(f_->clone(),
                                     next_ ? next_->clone() : NULL);
  }
  run_point<OUT>* add_segment(run_point<terminated>* p) {
    if (next_) {
      next_->add_segment(p);
    } else {
      next_ = p;
    }
    return this;
  }

  filter_base<terminated, OUT> *f_;
  run_point<terminated> *next_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  string get_id() const {
    if (this == NULL) {
      return "NULL";
    } else {
      std::stringstream result;
      result << "segment " << count_ << " filter_base [" << f_->get_id()
             << "] -> next " << next_->get_id();
      return result.str();
    }
  }

 private:
  // TODO(alasdair): Debug method - remove this or formalise it.
  void set_id() {
    count_ = ++segment_count;
  }
  int count_;
};

template <typename OUT>
void run_point<OUT>::Run(execution* pex) {
  throw;
}

template <>
void run_point<terminated>::Run(execution* pex) {
  pex->execute(f_);
  if(next_) {
    next_->Run(pex);
  }
}

template<typename MID,
         typename OUT>
run_point<OUT>* chain(const run_point<MID>* p, const filter_base<MID, OUT>* f) {
  filter_base<terminated, MID> *blah = p->f_;
  filter_base<terminated, MID> *f1 = blah->clone();
  filter_base<MID, OUT> *f2 = f->clone();
  filter_chain<terminated, MID, OUT> *fc =
      new filter_chain<terminated, MID, OUT>(f1, f2);
  return new run_point<OUT>(fc, NULL);
}

// END UTILITIES

// BEGIN CLASSES

template<typename IN,
         typename OUT>
class segment {
 public:
  segment(filter_base<IN, terminated> *leading,
          run_point<terminated> *chain,
          run_point<OUT> *trailing)
      : leading_(leading),
        chain_(chain),
        trailing_(trailing) {
    set_id();
  };

  ~segment() {
    if (leading_) {
      delete leading_;
    }
    if (chain_) {
      delete chain_;
    }
    if (trailing_) {
      delete trailing_;
    }
  };

  segment(const segment<IN, OUT>& p) {
    leading_ = p.leading_ ? p.leading_->clone() : NULL;
    chain_ = p.chain_ ? p.chain_->clone() : NULL;
    trailing_ = p.trailing_ ? p.trailing_->clone() : NULL;
  }
  segment(std::function<OUT (IN)>f);
  segment(std::function<void (IN, queue_front<OUT>)> f);

  typedef OUT out;
  typedef IN in;

  //
  // Implementation:
  //

  void run(execution* pex);
  filter_base<IN, terminated> *leading_clone() const {
    return leading_ ? leading_->clone() : NULL;
  }
  run_point<terminated> *chain_clone() const {
    return chain_ ? chain_->clone() : NULL;
  }
  run_point<OUT> *trailing_clone() const {
    return trailing_ ? trailing_->clone() : NULL;
  }

  segment<IN, OUT>* clone() const {
    return new segment<IN, OUT>(leading_clone(),
                                         chain_clone(),
                                         trailing_clone());
  }

  filter_base<IN, terminated> *leading_;
  run_point<terminated> *chain_;
  run_point<OUT> *trailing_;


  // TODO(alasdair): Debug method - remove this or formalise it.
  std::string get_id() {
    std::stringstream ss;
    ss << "segment-" << id_;
    return ss.str();
  }

 private:
  // TODO(alasdair): Debug method - remove this or formalise it.
  void set_id() {
    id_ = ++plan_id_count_;
  }
  int id_;
};

// END CLASSES

// BEGIN CONSTRUCTORS

template<typename OUT>
segment<terminated, OUT> from(queue_back<OUT> b) {
  run_point<OUT>* p =
      new run_point<OUT>(new filter_thread_point<OUT>(b), NULL);
  return segment<terminated, OUT>(NULL, NULL, p);
}

template<typename Q>
segment<terminated, typename Q::value_type> from(Q& q) {
  return from(q.back());
}

template<typename OUT>
segment<terminated, OUT> produce(
    std::function<void (queue_front<OUT>)> f) {
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( q, new queue_object<buffer_queue<OUT> >(
      10, get_q_name("produceq") ));
  filter_producer_point *fpp = new filter_producer_point(
      bind(call_and_close<OUT>, f, q));
  run_point<terminated>* p1 =
          new run_point<terminated>(
              fpp, NULL);
  run_point<OUT>* p2 =
      new run_point<OUT>(
          new filter_thread_point<OUT>(q), NULL);
  return segment<terminated, OUT>(NULL, p1, p2);
}

template<typename OUT>
segment<terminated, OUT> produce(void f(queue_front<OUT>)) {
  return produce(std::function<void (queue_front<OUT>)>(f));
}

template<typename IN,
         typename OUT>
segment<IN, OUT>::segment(std::function<OUT (IN)> f) {
  // TODO: ref counting queue, no limit
  typedef queue_object< buffer_queue<OUT> > qtype;
  qtype* q = new qtype(10, get_q_name("filterq"));

  void (queue_front<OUT>::*push)(const OUT&) = &queue_front<OUT>::push;
  std::function<void (OUT)> f1 = std::bind(push, q->front(),
                                           std::placeholders::_1);

  std::function<void (IN)> f2 =
      std::bind(chain<IN, OUT, void>, f, f1, std::placeholders::_1);

  std::function<terminated (IN)> f3 =  std::bind(do_consume<IN>, f2,
                                                   std::placeholders::_1);

  std::function<void ()> f4 = bind(&qtype::close, q);

  leading_ = new filter_function<IN, terminated>(f3, f4);
  chain_ = NULL;
  trailing_ = new run_point<OUT>(new filter_thread_point<OUT>(q->back()), NULL);
}

template<typename IN,
         typename OUT>
segment<IN, OUT>::segment(std::function<void (IN, queue_front<OUT>)> f) {
  typedef queue_object< buffer_queue<OUT> > qtype;
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( q, new qtype(10, get_q_name("filter_base")));
  // TODO: the queue is not deallocated
  std::function <void (IN)> ff = bind(f, std::placeholders::_1, q);
  std::function<terminated (IN)> fz =  std::bind(do_consume<IN>, ff,
                                                   std::placeholders::_1);
  std::function<void ()> f2 = bind(&qtype::close, q);

  leading_ = new filter_function<IN, terminated>(fz, f2);
  chain_ = NULL;
  trailing_ = new run_point<OUT>(new filter_thread_point<OUT>(q->back()), NULL);
}

template<typename IN>
segment<IN, terminated> consume(std::function<void (IN)> consumer,
                               std::function<void ()> close = nothing) {
  return segment<IN, terminated>(
      new filter_function<IN, terminated>(bind(do_consume<IN>, consumer,
                                                 std::placeholders::_1), close),
      NULL, NULL);
}

template<typename IN>
segment<IN, terminated> consume(void consumer(IN),
                               void close() = nothing) {
  return consume(static_cast<std::function<void (IN)> >(consumer),
                 static_cast<std::function<void ()> >(close));
}

template<typename IN>
segment<IN, terminated> to(queue_front<IN> front) {
  void (queue_front<IN>::*push)(const IN&) = &queue_front<IN>::push;
  std::function<void (IN)> f1 = std::bind(push, front,
                                          std::placeholders::_1);
  std::function<void ()> f2 = std::bind(&queue_front<IN>::close, front);
  return consume(f1, f2);
}

template<typename Q>
segment<typename Q::value_type, terminated> to(Q& q) {
  return to(q.front());
}


template<typename IN,
         typename OUT>
segment<IN, OUT> make_segment(std::function<OUT (IN)> f) {
  return segment<IN, OUT>(f);
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make_segment(OUT f(IN)) {
  return segment<IN, OUT>(std::function<OUT (IN)>(f));
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make_segment(std::function<void (IN, queue_front<OUT>)> f) {
  return segment<IN, OUT>(f);
}

template<typename IN,
         typename OUT>
segment<IN, OUT> make_segment(void f(IN, queue_front<OUT>)) {
  return segment<IN, OUT>(std::function<void (IN, queue_front<OUT>)>(f));
}

// This can take Full or Simple pipeline
template<typename T>
segment<typename T::in, typename T::out> parallel(T p) {
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( qp, new queue_object<buffer_queue<typename T::in> >(10) );

#if 0
  // TODO(alasdair): Fix Parallel test. The code as written gives us an access
  // violation because we are deleting a segment twice. The code in
  // this #if 0 block doesn't give the error, but never terminates. WTF?
  CXX0X_AUTO_VAR( src, from(qp->back()) | p);
  return to(qp->front()) | src;
#endif

  return to(qp->front()) | from(qp->back()) | p;
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
  run_point<terminated> *p =
      chain(p1.trailing_, p2.leading_);
  p->add_segment(p2.chain_clone());

  if (p1.chain_) {
    p = p1.chain_clone()->add_segment(p);
  }
  return segment<IN, OUT>(p1.leading_clone(),
                          p,
                          p2.trailing_clone());
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           std::function<OUT (MID)> f) {
  return p1 | make_segment(f);
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           OUT f(MID)) {
  return p1 | make_segment(f);
}

// disconnected pipe linker
template<typename IN,
         typename OUT>
segment<IN, OUT> operator|(
    const segment<IN, terminated>& p1,
    const segment<terminated, OUT>& p2) {
  run_point<terminated> *p = NULL;
  if (p1.chain_) {
    p = p1.chain_clone();
  }
  return segment<IN, OUT>(p1.leading_clone(),
                          p,
                          p2.trailing_clone());
}

// 1:N MID->OUT
template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           std::function<void (MID, queue_front<OUT>)> f) {
  return p1 | make_segment(f);
}

template<typename IN,
         typename MID,
         typename OUT>
segment<IN, OUT> operator|(const segment<IN, MID>& p1,
                           void f(MID, queue_front<OUT>)) {
  return p1 | make_segment(f);
}

// 1:0 MID->OUT
template<typename IN,
         typename MID>
segment<IN, terminated> operator|(const segment<IN, MID>& p1,
                                  std::function<void (MID)> f) {
  return p1 | consume(f);
}

template<typename IN,
         typename MID>
segment<IN, terminated> operator|(const segment<IN, MID>& p1,
                                  void f(MID)) {
  return p1 | consume(f);
}


template<typename MID,
         typename OUT>
segment<MID, terminated> operator|(queue_back<MID> back,
                                   const segment<MID, OUT>& p1) {
  return from(back) | p1;
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
                                  queue_front<MID> front) {
  return p1 | to(front);
}

template<typename IN,
         typename Q>
segment<IN, terminated> operator|(const segment<IN, typename Q::value_type>& p1,
                                  Q& q) {
  return p1 | to(q);
}

// END PIPES

// START PIPELINE IMPLEMENTATION

template<>
void segment<terminated, terminated>::run(
    execution* pex) {
  if(chain_) {
    chain_->Run(pex);
  }
  if (trailing_) {
    trailing_->Run(pex);
  }
}

// END PIPELINE IMPLEMENTATION

// START PIPELINE_EXECUTION IMPLEMENTATION

void RunFilter(execution* pex,
               filter_base<terminated, terminated> *f) {
  pex->start_.wait();
  bool b = true;
  while (b) {
    b = f->Run();
  }
  f->Close();

  pex->thread_end_->count_down_and_wait();
}

execution::execution(const plan& pp, simple_thread_pool* pool)
      : pp_(pp.clone()), pool_(pool),
        start_(1), thread_end_(NULL), end_(1),
        num_threads_(0), done_(false) {
  pp_->run(this);
  thread_end_ = new barrier(num_threads_,
                            std::bind(&execution::threads_done, this));
  start_.count_down();  // Start the threads
}

// TODO(aberkan): Should we allow the destructor to be non-blocking (i.e. thread
// semantics).  We'd have to manage resources in a separate object that would be
// shared by the pipeline and the execution objects.
execution::~execution() {
  wait();
  delete pp_;
  delete thread_end_;
}

void execution::execute(filter_base<terminated, terminated> *f) {
  num_threads_++;
  pool_->try_get_unused_thread()->execute(std::bind(RunFilter, this, f));
}


// END PIPELINE_EXECUTION IMPLEMENTATION


} // namespace pipeline

} // namespace gcl
#endif  // GCL_PIPELINE_
