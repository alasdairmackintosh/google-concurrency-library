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

// BEGIN UTILITIES
//TODO(aberkan): Common naming scheme
//TODO(aberkan): Better commenting
//TODO(aberkan): Move some of this out of this file
//TODO(aberkan): Simplify filter and segment stuff
//TODO(aberkan): Move excecution strategy to template parameter
//TODO(aberkan): Reduce Cloning
// TODO(alasdair): Remove debug methods (principally names for instances), or
// make them formal and document them. Currently debugging pipelines is tricky,
// so we are keeping these for now.

// BEGIN DEBUGGING VARIABLES
// Static variables used for assigning names and IDs to various
// pipeline elements. These are very messy.

std::atomic<int> FullPipelinePlan_id_count_(0);
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


class PipelineTerm { };

template <typename T>
PipelineTerm ignore(T t) { return PipelineTerm(); };

template<typename T>
PipelineTerm do_consume(std::function<void (T)> f, T t) {
  f(t);
  return PipelineTerm();
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
class filter {
 public:
  filter() {
    set_id();
  }
  virtual ~filter() {};
  virtual OUT Apply(IN in) = 0;
  virtual bool Run(std::function<PipelineTerm (OUT)> r) = 0;
  virtual bool Run() = 0;
  virtual void Close() = 0;
  virtual filter<IN, OUT>* clone() const = 0;
  // TODO(alasdair): Debug methods - remove this or formalise it.
  virtual string get_id() const = 0;
 protected:
  void set_id() {
    id_ = ++filter_count;
  }
  int id_;
};

template <typename IN, typename OUT>
class FullPipelinePlan;

typedef FullPipelinePlan<PipelineTerm, PipelineTerm> PipelinePlan;

class PipelineExecution {
 public:
  PipelineExecution(const PipelinePlan& pp, simple_thread_pool* pool);

  ~PipelineExecution();

  bool is_done() {
    return done_;
  }
  void wait() {
    end_.wait();
    thread_end_->count_down_and_wait();
    assert(is_done());
  }
  void cancel();

  void execute(filter<PipelineTerm, PipelineTerm> *f);

  void threads_done() {
    // This method is invoked after all threads have called
    // count_down_and_wait(), but not before they have all exited. To ensure
    // that a caller cannot return from wait() until all threads have exited
    // count_down_and_wait(), we reset the barrier, and re-use it for a final
    // test in PipelineExecution::wait(). We clear the callback method for the
    // barrier so that this doesn't get called again.
    done_ = true;
    thread_end_->reset(static_cast<size_t>(1));
    thread_end_->reset(static_cast<std::function<void ()> >(NULL));
    end_.count_down();
  }

  PipelinePlan* pp_;
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
class filter_chain : public filter<IN, OUT> {
 public:
  filter_chain(class filter<IN, MID> *first,
               class filter<MID, OUT> *second)
      : first_(first),
        second_(second) {assert(first && second);};
  virtual ~filter_chain() {
    delete first_;
    delete second_;
  };
  virtual OUT Apply(IN in) {
    return second_->Apply(first_->Apply(in));
  };
  virtual filter<IN, OUT>* clone() const {
    return new filter_chain<IN, MID, OUT>(first_->clone(), second_->clone());
  };
  //TODO(aberkan): Make calling run on non-runnable a compile time error.
  virtual bool Run(std::function<PipelineTerm (OUT)> r);
  virtual bool Run();
  virtual void Close() {
    first_->Close();
    second_->Close();
  }
  class filter<IN, MID>* first_;
  class filter<MID, OUT>* second_;

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
bool filter_chain<IN, MID, OUT>::Run(std::function<PipelineTerm (OUT)> r) {
  std::function<OUT (MID)> m = std::bind(&filter<MID, OUT>::Apply, second_,
                                         std::placeholders::_1);
  std::function<PipelineTerm (MID)> p
    = std::bind(chain<MID, OUT, PipelineTerm>, m, r, std::placeholders::_1);
  return first_->Run(p);
};

template <typename IN,
          typename MID,
          typename OUT>
bool filter_chain<IN, MID, OUT>::Run() {
  std::function<OUT (MID)> m = std::bind(&filter<MID, OUT>::Apply, second_,
                                         std::placeholders::_1);
  std::function<PipelineTerm (OUT)> r = ignore<OUT>;
  std::function<PipelineTerm (MID)> p =
    std::bind(chain<MID, OUT, PipelineTerm>, m, r, std::placeholders::_1);
  return first_->Run(p);
};

void Nothing() {};

template <typename IN,
          typename OUT>
class filter_function : public filter<IN, OUT> {
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
  virtual bool Run(std::function<PipelineTerm (OUT)> r) {
    throw;
  }
  virtual bool Run() {
    throw;
  }
  virtual void Close() {
    close_();
  }
  virtual class filter<IN, OUT>* clone() const {
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
class filter_thread_point : public filter<PipelineTerm, OUT> {
 public:
  filter_thread_point(queue_back<OUT> qb) : qb_(qb), name_(qb.name()) {
    set_id();
  };
  ~filter_thread_point() { };
  virtual OUT Apply(PipelineTerm IN) {
    throw;
  }
  virtual bool Run(std::function<PipelineTerm (OUT)> r) {
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

  virtual filter<PipelineTerm, OUT>* clone() const {
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

class filter_producer_point : public filter<PipelineTerm, PipelineTerm> {
 public:
  filter_producer_point(std::function<void ()> f) : f_(f) { };
  virtual PipelineTerm Apply(PipelineTerm) {
    throw;
  }
  virtual bool Run(std::function<PipelineTerm (PipelineTerm)> r) {
    throw;
  }
  virtual bool Run() {
    f_();
    return false;
  }

  virtual void Close() { };
  virtual filter<PipelineTerm, PipelineTerm>* clone() const {
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
class pipeline_segment {
 public:
  pipeline_segment(filter<PipelineTerm, OUT> *f,
                   pipeline_segment<PipelineTerm> *next)
      : f_(f), next_(next) {
    set_id();
  };
  virtual ~pipeline_segment() {
    delete f_;
    delete next_;
  }
  void Run(PipelineExecution* pex);
  pipeline_segment<OUT> *clone() const {
    return new pipeline_segment<OUT>(f_->clone(),
                                     next_ ? next_->clone() : NULL);
  }
  pipeline_segment<OUT>* add_segment(pipeline_segment<PipelineTerm>* p) {
    if (next_) {
      next_->add_segment(p);
    } else {
      next_ = p;
    }
    return this;
  }

  filter<PipelineTerm, OUT> *f_;
  pipeline_segment<PipelineTerm> *next_;

  // TODO(alasdair): Debug method - remove this or formalise it.
  string get_id() const {
    if (this == NULL) {
      return "NULL";
    } else {
      std::stringstream result;
      result << "segment " << count_ << " filter [" << f_->get_id()
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
void pipeline_segment<OUT>::Run(PipelineExecution* pex) {
  throw;
}

template <>
void pipeline_segment<PipelineTerm>::Run(PipelineExecution* pex) {
  pex->execute(f_);
  if(next_) {
    next_->Run(pex);
  }
}

template<typename MID,
         typename OUT>
pipeline_segment<OUT>* chain(const pipeline_segment<MID>* p,
                             const filter<MID, OUT>* f) {
  pipeline_segment<OUT> *ps = new pipeline_segment<OUT>(
      new filter_chain<PipelineTerm, MID, OUT>(p->f_->clone(),
                                               f->clone()),
      NULL);
  return ps;
}

// END UTILITIES

// BEGIN CLASSES

template<typename IN,
         typename OUT>
class FullPipelinePlan {
 public:
  FullPipelinePlan(filter<IN, PipelineTerm> *leading,
                   pipeline_segment<PipelineTerm> *chain,
                   pipeline_segment<OUT> *trailing)
      : leading_(leading),
        chain_(chain),
        trailing_(trailing) {
    set_id();
  };

  ~FullPipelinePlan() {
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
  void run(PipelineExecution* pex);
  filter<IN, PipelineTerm> *leading_clone() const {
    return leading_ ? leading_->clone() : NULL;
  }
  pipeline_segment<PipelineTerm> *chain_clone() const {
    return chain_ ? chain_->clone() : NULL;
  }
  pipeline_segment<OUT> *trailing_clone() const {
    return trailing_ ? trailing_->clone() : NULL;
  }

  FullPipelinePlan<IN, OUT>* clone() const {
    return new FullPipelinePlan<IN, OUT>(leading_clone(),
                                         chain_clone(),
                                         trailing_clone());
  }

  filter<IN, PipelineTerm> *leading_;
  pipeline_segment<PipelineTerm> *chain_;
  pipeline_segment<OUT> *trailing_;

  typedef OUT out;
  typedef IN in;


  // TODO(alasdair): Debug method - remove this or formalise it.
  std::string get_id() {
    std::stringstream ss;
    ss << "FullPipelinePlan-" << id_;
    return ss.str();
  }

 private:
  // TODO(alasdair): Debug method - remove this or formalise it.
  void set_id() {
    id_ = ++FullPipelinePlan_id_count_;
  }
  int id_;
};

// END CLASSES

// BEGIN CONSTRUCTORS

template<typename OUT>
FullPipelinePlan<PipelineTerm, OUT> Source(queue_back<OUT> b) {
  pipeline_segment<OUT>* p =
      new pipeline_segment<OUT>(new filter_thread_point<OUT>(b), NULL);
  return FullPipelinePlan<PipelineTerm, OUT>(NULL, NULL, p);
}

template<typename OUT>
FullPipelinePlan<PipelineTerm, OUT> Produce(
    std::function<void (queue_front<OUT>)> f) {
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( q, new queue_object<buffer_queue<OUT> >(
      10, get_q_name("produceq") ));
  filter_producer_point *fpp = new filter_producer_point(
      bind(call_and_close<OUT>, f, q));
  pipeline_segment<PipelineTerm>* p1 =
          new pipeline_segment<PipelineTerm>(
              fpp, NULL);
  pipeline_segment<OUT>* p2 =
      new pipeline_segment<OUT>(
          new filter_thread_point<OUT>(q), NULL);
  return FullPipelinePlan<PipelineTerm, OUT>(NULL, p1, p2);
}

template<typename IN,
         typename OUT>
FullPipelinePlan<IN, OUT> Filter(std::function<OUT (IN)>f) {
  // TODO: ref counting queue, no limit
  typedef queue_object< buffer_queue<OUT> > qtype;
  qtype* q = new qtype(10, get_q_name("filterq"));

  void (queue_front<OUT>::*push)(const OUT&) = &queue_front<OUT>::push;
  std::function<void (OUT)> f1 = std::bind(push, q->front(),
                                           std::placeholders::_1);

  std::function<void (IN)> f2 =
      std::bind(chain<IN, OUT, void>, f, f1, std::placeholders::_1);

  std::function<PipelineTerm (IN)> f3 =  std::bind(do_consume<IN>, f2,
                                                   std::placeholders::_1);

  std::function<void ()> f4 = bind(&qtype::close, q);

  return FullPipelinePlan<IN, OUT>(
      new filter_function<IN, PipelineTerm>(f3, f4), NULL,
      new pipeline_segment<OUT>(
          new filter_thread_point<OUT>(q->back()), NULL));
}

template<typename IN,
         typename OUT>
FullPipelinePlan<IN, OUT> Filter(OUT f(IN)) {
    return Filter(static_cast<std::function<OUT (IN)> >(f));
}

template<typename IN,
         typename OUT>
FullPipelinePlan<IN, OUT> Filter(
    std::function<void (IN, queue_front<OUT>)> f) {
  typedef queue_object< buffer_queue<OUT> > qtype;
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( q, new qtype(10, get_q_name("filter")));
  // TODO: the queue is not deallocated
  std::function <void (IN)> ff = bind(f, std::placeholders::_1, q);
  std::function<void ()> f2 = bind(&qtype::close, q);
  return Consume(ff, f2) | Source(q->back());
}

template<typename IN>
FullPipelinePlan<IN, PipelineTerm> Consume(std::function<void (IN)> consumer,
                                           std::function<void ()> close) {
  return FullPipelinePlan<IN, PipelineTerm>(
      new filter_function<IN, PipelineTerm>(bind(do_consume<IN>, consumer,
                                                 std::placeholders::_1), close),
      NULL, NULL);
}

template<typename IN>
FullPipelinePlan<IN, PipelineTerm> Consume(void consumer(IN),
                                           void close()) {
  return Consume(static_cast<std::function<void (IN)> >(consumer),
                 static_cast<std::function<void ()> >(close));
}

template<typename IN>
FullPipelinePlan<IN, PipelineTerm> SinkAndClose(queue_front<IN> front) {
  void (queue_front<IN>::*push)(const IN&) = &queue_front<IN>::push;
  std::function<void (IN)> f1 = std::bind(push, front,
                                          std::placeholders::_1);
  std::function<void ()> f2 = std::bind(&queue_front<IN>::close, front);
  return Consume(f1, f2);
}

// This can take Full or Simple PipelinePlans
template<typename T>
FullPipelinePlan<typename T::in, typename T::out> Parallel(T p) {
  // TODO: ref counting queue, no limit
  CXX0X_AUTO_VAR( qp, new queue_object<buffer_queue<typename T::in> >(10) );

#if 0
  // TODO(alasdair): Fix Parallel test. The code as written gives us an access
  // violation because we are deleting a FullPipelinePlan twice. The code in
  // this #if 0 block doesn't give the error, but never terminates. WTF?
  CXX0X_AUTO_VAR( src, Source(qp->back()) | p);
  return SinkAndClose(qp->front()) | src;
#endif

  return SinkAndClose(qp->front()) | Source(qp->back()) | p;
}

// END CONSTRUCTORS

// BEGIN PIPES
template<typename IN,
         typename MID,
         typename OUT>
FullPipelinePlan<IN, OUT> operator|(const FullPipelinePlan<IN, MID>& p1,
                                    const FullPipelinePlan<MID, OUT>& p2) {
  pipeline_segment<PipelineTerm> *p =
      chain(p1.trailing_, p2.leading_)->add_segment(p2.chain_clone());
  if (p1.chain_) {
    p = p1.chain_clone()->add_segment(p);
  }
  return FullPipelinePlan<IN, OUT>(p1.leading_clone(),
                               p,
                               p2.trailing_clone());
}

template<typename IN,
         typename OUT>
FullPipelinePlan<IN, OUT> operator|(
    const FullPipelinePlan<IN, PipelineTerm>& p1,
    const FullPipelinePlan<PipelineTerm, OUT>& p2) {
  pipeline_segment<PipelineTerm> *p = NULL;
  if (p1.chain_) {
    p = p1.chain_clone();
  }
  return FullPipelinePlan<IN, OUT>(p1.leading_clone(),
                                   p,
                                   p2.trailing_clone());
}

// END PIPES

// START PIPELINE IMPLEMENTATION

template<>
void FullPipelinePlan<PipelineTerm, PipelineTerm>::run(
    PipelineExecution* pex) {
  if(chain_) {
    chain_->Run(pex);
  }
  if (trailing_) {
    trailing_->Run(pex);
  }
}

// END PIPELINE IMPLEMENTATION

// START PIPELINE_EXECUTION IMPLEMENTATION

void RunFilter(PipelineExecution* pex,
               filter<PipelineTerm, PipelineTerm> *f) {
  pex->start_.wait();
  bool b = true;
  while (b) {
    b = f->Run();
  }
  f->Close();

  pex->thread_end_->count_down_and_wait();
}

PipelineExecution::PipelineExecution(const PipelinePlan& pp,
                                     simple_thread_pool* pool)
      : pp_(pp.clone()), pool_(pool),
        start_(1), thread_end_(NULL), end_(1),
        num_threads_(0), done_(false) {
  pp_->run(this);
  thread_end_ = new barrier(num_threads_,
                            std::bind(&PipelineExecution::threads_done, this));
  start_.count_down();  // Start the threads
}

PipelineExecution::~PipelineExecution() {
  wait();
  delete pp_;
  delete thread_end_;
}

void PipelineExecution::execute(filter<PipelineTerm, PipelineTerm> *f) {
  num_threads_++;
  pool_->try_get_unused_thread()->execute(std::bind(RunFilter, this, f));
}


// END PIPELINE_EXECUTION IMPLEMENTATION



} // namespace
#endif  // GCL_PIPELINE_
