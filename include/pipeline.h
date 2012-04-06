// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_PIPELINE_
#define GCL_PIPELINE_

#include <exception>
#include <tr1/functional>

#include <simple_thread_pool.h>

using std::tr1::function;
using std::tr1::bind;
using std::tr1::placeholders::_1;

namespace gcl {

// TODO(alasdair): This API is incomplete, and subject to change.
// Submitting as-is in order to provide a framework for future
// discussion and development.

// Chains an in->intermediate function with an intermediate->out function
// to make an in->out function
template <typename IN_TYPE,
          typename INTERMEDIATE,
          typename OUT_TYPE>
static OUT_TYPE chain(function<INTERMEDIATE (IN_TYPE in)> intermediate_fn,
                      function<OUT_TYPE (INTERMEDIATE in)> out_fn,
                      IN_TYPE in) {
  return out_fn(intermediate_fn(in));
}

// Chains an in->intermediate function with an intermediate consumer
// to make an in consumer
template <typename IN_TYPE,
          typename INTERMEDIATE>
static void terminate(function<INTERMEDIATE (IN_TYPE in)> intermediate_fn,
                     function<void (INTERMEDIATE in)> out_fn,
                     IN_TYPE in) {
  out_fn(intermediate_fn(in));
}

template <typename IN,
          typename OUT,
          typename SOURCE>
class StartablePipeline;

template <typename IN,
          typename OUT,
          typename SOURCE>
class RunnablePipeline;

template <typename IN,
          typename OUT = IN>
class Pipeline {
 public:

  Pipeline(function<OUT (IN input)> fn) : fn_(fn) {
  }

  Pipeline(const Pipeline& other) : fn_(other.fn_) {
  }

  Pipeline& operator=(const Pipeline& other) {
    fn_ = other.fn_;
    return *this;
  }

  template <typename NEW_OUT>
  Pipeline<IN, NEW_OUT> Filter(function<NEW_OUT (OUT input)>filter) {
    function<NEW_OUT (IN input)>chain_fn = bind(chain<IN, OUT, NEW_OUT>, fn_, filter, _1);
     return Pipeline<IN, NEW_OUT>(chain_fn);
  }

  template <typename SOURCE>
  StartablePipeline<IN, OUT, SOURCE> Source(SOURCE* source);

  OUT apply(IN in) {
    return fn_(in);
  }

 protected:
  Pipeline() : fn_(NULL) {
  }

  function<OUT (IN input)> get_fn() {
    return fn_;
  }

 private:
  function<OUT (IN input)> fn_;
};

// StartablePipeline - adds a source
template <typename IN,
          typename OUT,
          typename SOURCE>
class StartablePipeline : public Pipeline<IN, OUT> {
 public:
  StartablePipeline(const Pipeline<IN, OUT>& pipeline, SOURCE* source)
      : Pipeline<IN, OUT>(pipeline), source_(source) {
  }

  StartablePipeline(SOURCE* source)
      : source_(source) {
  }

  StartablePipeline(const StartablePipeline& other)
      : Pipeline<IN, OUT>(other),
        source_(other.source_) {
  }

  void operator=(const StartablePipeline<IN, OUT, SOURCE>& other) {
    Pipeline<IN, OUT>::operator=(other);
    source_ = other.source_;
    return *this;
  }

  using Pipeline<IN,OUT>::get_fn;
  template <typename NEW_OUT>
  StartablePipeline<IN, NEW_OUT, SOURCE> Filter(function<NEW_OUT (OUT input)>filter) {
    function<NEW_OUT (IN input)> chain_fn = bind(chain<IN, OUT, NEW_OUT>, get_fn(), filter, _1);
    return StartablePipeline<IN, NEW_OUT, SOURCE>(chain_fn);
  }

  RunnablePipeline<IN, OUT, SOURCE> Consume(function<void (OUT result)> sink);

 protected:
  SOURCE* get_source() {
    return source_;
  }
 private:
  SOURCE* source_;

};

// RunnablePipeline - adds a consumer, and provides a run() method
template <typename IN,
          typename OUT,
          typename SOURCE>
class RunnablePipeline : public StartablePipeline<IN, OUT, SOURCE> {
 public:
  RunnablePipeline(StartablePipeline<IN, OUT, SOURCE>& start, function<void (IN input)> consumer)
  : StartablePipeline<IN, OUT, SOURCE>(start), consumer_(consumer) {
  }

  RunnablePipeline(SOURCE* source)
    : StartablePipeline<IN, OUT, SOURCE>(source), consumer_(NULL) {
  }

  RunnablePipeline(SOURCE* source, function<void (OUT output)> consumer)
    : StartablePipeline<IN, OUT, SOURCE>(source), consumer_(consumer) {
  }

  using StartablePipeline<IN, OUT, SOURCE>::get_source;
  void run() {
    bool closed = false;
    while(!closed) {
      get_source()->wait();
      closed = get_source()->is_closed();
      if (!closed) {
        consumer_(get_source()->get());
      }
    }
  }

  // Runs this pipeline in a threadpool.
  // TODO(alasdair): What kind of threadpool do we support?
  // TODO(alasdair): How do we handle errors?
  void run(simple_thread_pool& pool) {
    mutable_thread* thread = pool.try_get_unused_thread();
    if (thread != NULL) {
      function <void ()> f = bind(static_cast<void (RunnablePipeline::*)()>(
          &RunnablePipeline<IN, OUT, SOURCE>::run), this);
      if (!thread->execute(f)) {
        throw std::exception();
      }
    } else {
      throw std::exception();
    }
  }
 private:
  function <void(IN input)> consumer_;
};

template <typename IN,
          typename OUT>
template <typename SOURCE>
StartablePipeline<IN, OUT, SOURCE> Pipeline<IN, OUT>::Source(SOURCE* source) {
  return StartablePipeline<IN, OUT, SOURCE>(*this, source);
}

template <typename IN,
          typename OUT,
          typename SOURCE>
RunnablePipeline<IN, OUT, SOURCE> StartablePipeline<IN, OUT, SOURCE>::Consume(
    function <void(OUT output)> consumer) {
  function<OUT (IN in)> fn = get_fn();
  function<void (IN in)> consumer_fn = bind(terminate<IN, OUT>, fn, consumer, _1);
  return RunnablePipeline<IN, OUT, SOURCE>(*this, consumer_fn);
}

}
#endif  // GCL_PIPELINE_
