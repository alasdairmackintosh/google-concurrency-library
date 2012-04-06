// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_PIPELINE_
#define GCL_PIPELINE_

#include <iostream>
#include <vector>

#include <exception>
#include <stdexcept>
#include <tr1/functional>

#include <atomic.h>
#include <queue_base.h>
#include <countdown_latch.h>
#include <simple_thread_pool.h>

using std::tr1::function;
using std::tr1::bind;
using std::vector;
using std::tr1::placeholders::_1;


namespace gcl {

// BEGIN UTILITIES

class PipelineTerm { };

template <typename IN_TYPE,
          typename INTERMEDIATE,
          typename OUT_TYPE>
static OUT_TYPE chain(function<INTERMEDIATE (IN_TYPE in)> intermediate_fn,
                      function<OUT_TYPE (INTERMEDIATE in)> out_fn,
                      IN_TYPE in) {
  return out_fn(intermediate_fn(in));
}

template <typename T>
PipelineTerm ignore(T t) { return PipelineTerm(); };

template<typename T>
PipelineTerm do_consume(function<void (T)> f, T t) {
  f(t);
  return PipelineTerm();
}

template <typename IN,
          typename OUT>
class filter {
 public:
  virtual ~filter() {};
  virtual OUT Apply(IN in) = 0;
  virtual bool Run(function<PipelineTerm (OUT)> r) = 0;
  virtual bool Run() = 0;
  virtual filter<IN, OUT>* Clone() const = 0;
};

template <typename IN,
          typename MID,
          typename OUT>
class filter_chain : public filter<IN, OUT> {
 public:
  filter_chain(class filter<IN, MID> *first,
               class filter<MID, OUT> *second)
      : first_(first),
        second_(second) {};
  virtual ~filter_chain() {
    delete first_;
    delete second_;
  };
  virtual OUT Apply(IN in) {
    return second_->Apply(first_->Apply(in));
  };
  virtual filter<IN, OUT>* Clone() const {
    return new filter_chain<IN, MID, OUT>(first_->Clone(), second_->Clone());
  };
  virtual bool Run(function<PipelineTerm (OUT)> r);
  virtual bool Run();
  class filter<IN, MID>* first_;
  class filter<MID, OUT>* second_;
};

template <typename IN,
          typename MID,
          typename OUT>
bool filter_chain<IN, MID, OUT>::Run(function<PipelineTerm (OUT)> r) {
  function<OUT (MID)> m = bind(&filter<MID, OUT>::Apply, second_, _1);
  function<PipelineTerm (MID)> p = bind(chain<MID, OUT, PipelineTerm>, m, r, _1);
  return first_->Run(p);
};

template <typename IN,
          typename MID,
          typename OUT>
bool filter_chain<IN, MID, OUT>::Run() {
  function<OUT (MID)> m = bind(&filter<MID, OUT>::Apply, second_, _1);
  function<PipelineTerm (OUT)> r = ignore<OUT>;
  function<PipelineTerm (MID)> p = bind(chain<MID, OUT, PipelineTerm>, m, r, _1);
  return first_->Run(p);
};

template <typename IN,
          typename OUT>
class filter_function : public filter<IN, OUT> {
 public:
  filter_function(function<OUT (IN)> f)
      : f_(f) { };
  virtual ~filter_function() { };
  virtual OUT Apply(IN in) {
    return f_(in);
  }
  virtual bool Run(function<PipelineTerm (OUT)> r) {
    throw;
  }
  virtual bool Run() {
    throw;
  }
  virtual class filter<IN, OUT>* Clone() const {
    return new filter_function<IN, OUT>(f_);
  }
  function<OUT (IN)> f_;
};

template <typename OUT>
class filter_thread_point : public filter<PipelineTerm, OUT> {
 public:
  filter_thread_point(queue_back<OUT> *qb) : qb_(qb) { };
  ~filter_thread_point() { };
  virtual OUT Apply(PipelineTerm IN) {
    throw;
    return qb_->pop();
  }
  virtual bool Run(function<PipelineTerm (OUT)> r) {
    OUT out;
    queue_op_status status = qb_->wait_pop(out);
    if (status != success) {
      return false;
    }
    r(out);
    return true;
  }
  virtual bool Run() {
    throw;
  }

  virtual filter<PipelineTerm, OUT>* Clone() const {
    return new filter_thread_point<OUT>(qb_);
  }
  queue_back<OUT> *qb_;
};

void RunFilter(filter<PipelineTerm, PipelineTerm> *f) {
  // Todo quitting stuff...
  bool b = true;
  while (b) {
    b = f->Run();
  }
}

template <typename OUT>
class pipeline_segment {
 public:
  pipeline_segment(filter<PipelineTerm, OUT> *f,
                   pipeline_segment<PipelineTerm> *next)
      : f_(f), next_(next) { };
  virtual ~pipeline_segment() {
    delete f_;
    delete next_;
  }
  void Run(simple_thread_pool& pool);
  pipeline_segment<OUT> *Clone() const {
    return new pipeline_segment<OUT>(f_->Clone(), next_->Clone());
  }

  filter<PipelineTerm, OUT> *f_;
  pipeline_segment<PipelineTerm> *next_;

};

template <typename OUT>
void pipeline_segment<OUT>::Run(simple_thread_pool& pool) {
  throw;
}
template <>
void pipeline_segment<PipelineTerm>::Run(simple_thread_pool& pool) {
  pool.try_get_unused_thread()->
      execute(bind(RunFilter, f_));
  if(next_) {
    next_->Run(pool);
  }
}

template<typename MID,
         typename OUT>
pipeline_segment<OUT>* Chain(const pipeline_segment<MID>* p,
                             const filter<MID, OUT>* f) {
  pipeline_segment<OUT> *ps = new pipeline_segment<OUT>(
      new filter_chain<PipelineTerm, MID, OUT>(p->f_->Clone(),
                                               f->Clone()),
      NULL);
  return ps;
}

// END UTILITIES

// BEGIN CLASSES

template<typename IN,
         typename OUT>
class FullPipeline {
 public:
  FullPipeline(filter<IN, PipelineTerm> *leading,
               pipeline_segment<PipelineTerm> *chain,
               pipeline_segment<OUT> *trailing)
      : leading_(leading),
        chain_(chain),
        trailing_(trailing) {
  };

  ~FullPipeline() {
    delete leading_;
    delete chain_;
    delete trailing_;
  };
  void run(simple_thread_pool& pool);
  filter<IN, PipelineTerm> *leading_clone() const {
    return leading_ ? leading_->Clone() : NULL;
  }
  pipeline_segment<PipelineTerm> *chain_clone() const {
    return chain_ ? chain_->Clone() : NULL;
  }
  pipeline_segment<OUT> *trailing_clone() const {
    return trailing_ ? trailing_->Clone() : NULL;
  }

  filter<IN, PipelineTerm> *leading_;
  pipeline_segment<PipelineTerm> *chain_;
  pipeline_segment<OUT> *trailing_;
};

template<typename IN,
         typename OUT>
class SimplePipeline {
 public:
  SimplePipeline(function<OUT (IN in)> f)
      : f_(new filter_function<IN, OUT>(f)) { };
  SimplePipeline(filter<IN, OUT> *f)
      : f_(f) { };

  OUT Apply(IN in) {
    return f_->Apply(in);
  }

  filter<IN, OUT> *f_;
};

typedef FullPipeline<PipelineTerm, PipelineTerm> Pipeline;

// END CLASSES

// BEGIN CONSTRUCTORS

template<typename OUT>
FullPipeline<PipelineTerm, OUT> Source(queue_back<OUT> *b) {
  pipeline_segment<OUT>* p =
      new pipeline_segment<OUT>(new filter_thread_point<OUT>(b), NULL);
  return FullPipeline<PipelineTerm, OUT>(NULL, NULL, p);
}

template<typename IN,
         typename OUT>
SimplePipeline<IN, OUT> Filter(OUT f(IN)) {
  return SimplePipeline<IN, OUT>(f);
}

template<typename IN,
         typename OUT>
SimplePipeline<IN, OUT> Filter(function<OUT (IN)> f) {
  return SimplePipeline<IN, OUT>(f);
}

template<typename IN>
SimplePipeline<IN, PipelineTerm> Consume(void consumer(IN)) {
  return function<PipelineTerm (IN)>(bind(do_consume<IN>, consumer, _1));
}

template<typename IN>
SimplePipeline<IN, PipelineTerm> Consume(function<void (IN)> consumer) {
  return function<PipelineTerm (IN)>(bind(do_consume<IN>, consumer, _1));
}

template<typename IN>
SimplePipeline<IN, PipelineTerm> Sink(queue_front<IN> front) {
  function<void (IN)> f1 = bind(&queue_front<IN>::put, front, _1);
  return Consume(f1);
}

// END CONSTRUCTORS

// BEGIN PIPES

template<typename IN,
         typename MID,
         typename OUT>
SimplePipeline<IN, OUT> operator|(const SimplePipeline<IN, MID>& p1,
                                  const SimplePipeline<MID, OUT>& p2) {
  return SimplePipeline<IN, OUT>(
      new filter_chain<IN, MID, OUT>(p1.f_->Clone(),
                                     p2.f_->Clone()));
}

template<typename IN,
         typename MID,
         typename OUT>
FullPipeline<IN, OUT> operator|(const FullPipeline<IN, MID>& p1,
                                const SimplePipeline<MID, OUT>& p2) {
  return FullPipeline<IN, OUT>(p1.leading_clone(),
                               p1.chain_clone(),
                               Chain(p1.trailing_, p2.f_));
}

template<typename IN,
         typename MID,
         typename OUT>
FullPipeline<IN, OUT> operator|(const SimplePipeline<IN, MID>& p1,
                                const FullPipeline<MID, OUT>& p2) {
  filter<IN, PipelineTerm> *leading =
      filter_chain<IN, MID, PipelineTerm>(p1.f_, p2.leading_);
  return FullPipeline<IN, OUT>(leading,
                               p2.chain_clone(),
                               p2.trailing_clone());
}

template<typename IN,
         typename MID,
         typename OUT>
FullPipeline<IN, OUT> operator|(const FullPipeline<IN, MID>& p1,
                                const FullPipeline<MID, OUT>& p2) {
  pipeline_segment<PipelineTerm> *p =
      Chain(p1.trailing_, p2.leading_)->chain(p2->chain_clone());
  if (p1.chain_) {
    p = p1.chain_clone()->chain(p);
  }
  return FullPipeline<IN, OUT>(p1.leading_clone(),
                               p,
                               p2.trailing_clone());
}

// END PIPES

template<>
void FullPipeline<PipelineTerm, PipelineTerm>::run(simple_thread_pool& pool) {
  if(chain_) {
    chain_->Run(pool);
  }
  trailing_->Run(pool);
}
}
#endif  // GCL_PIPELINE_
