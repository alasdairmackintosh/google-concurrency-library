// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef GCL_PIPELINE_
#define GCL_PIPELINE_

#include <iostream>

#include <exception>
#include <stdexcept>
#include <tr1/functional>

#include <atomic.h>
#include <countdown_latch.h>
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
      : StartablePipeline<IN, OUT, SOURCE>(start),
        n_threads_(0),
        consumer_(consumer),
        end_latch_(1) {
    set_default_end_fn();
  }

  RunnablePipeline(SOURCE* source)
      : StartablePipeline<IN, OUT, SOURCE>(source),
        n_threads_(0),
        consumer_(NULL),
        end_latch_(1) {
    set_default_end_fn();
  }

  RunnablePipeline(SOURCE* source, function<void (OUT output)> consumer)
    : StartablePipeline<IN, OUT, SOURCE>(source),
      n_threads_(0),
      consumer_(consumer),
      end_latch_(1) {
    set_default_end_fn();
  }

  RunnablePipeline(const RunnablePipeline& other)
      : StartablePipeline<IN, OUT, SOURCE>(other),
        n_threads_(other.n_threads_),
        consumer_(other.consumer_),
        end_latch_(1) {
    set_end_fn(other);
  }

 protected:
  RunnablePipeline(const RunnablePipeline& other, function<void ()> end_fn)
      : StartablePipeline<IN, OUT, SOURCE>(other),
        n_threads_(other.n_threads_),
        consumer_(other.consumer_),
        end_fn_(end_fn),
        default_end_(false),
        end_latch_(0) {
  }

  RunnablePipeline(const RunnablePipeline& other, int n_threads)
      : StartablePipeline<IN, OUT, SOURCE>(other),
        n_threads_(n_threads),
        consumer_(other.consumer_),
        end_latch_(1) {
    set_end_fn(other);
  }

  void set_end_fn(const RunnablePipeline& other) {
    if (other.default_end_) {
      set_default_end_fn();
    } else {
      end_fn_ = other.end_fn_;
      default_end_ = false;
    }
  }

 public:
  // Sets an explicit function to be called when the Pipeline ends.
  // Cannot be combined with wait()
  RunnablePipeline<IN, OUT, SOURCE> OnEnd(function<void ()> end_fn) {
    return RunnablePipeline<IN, OUT, SOURCE>(*this, end_fn);
  }

  RunnablePipeline<IN, OUT, SOURCE> Parallel(int n_threads) {
    return RunnablePipeline<IN, OUT, SOURCE>(*this, n_threads);
  }

  void run() {
    if (n_threads_ > 0) {
      std::cerr << "Warning - running in serial mode\n";
    }
    run_internal();
  }

  // Runs this pipeline in a threadpool.
  // TODO(alasdair): What kind of threadpool do we support?
  // TODO(alasdair): How do we handle errors?
  void run(simple_thread_pool& pool) {
    if (n_threads_ > 0) {
      atomic_init(&count_, n_threads_);
    }
    int thread_count = n_threads_ < 1 ? 1 : n_threads_;
    for (int i = 0; i < thread_count; i++) {
      mutable_thread* thread = pool.try_get_unused_thread();
      if (thread != NULL) {
        function <void ()> f = bind(static_cast<void (RunnablePipeline::*)()>(
            &RunnablePipeline<IN, OUT, SOURCE>::run_internal), this);
        if (!thread->execute(f)) {
          // TODO(alasdair): What do we do here?
          throw std::exception();
        }
      } else {
        // TODO(alasdair): What do we do here?
        throw std::exception();
      }
    }
  }

  // Blocks until the pipeline has finished. Throws an exception if a
  // custom function was defined using OnEnd
  void wait()  throw (std::logic_error) {
    if (!default_end_) {
      throw std::logic_error("Cannot wait if EndFn defined.");
    }
    end_latch_.wait();
  }
 private:

  using StartablePipeline<IN, OUT, SOURCE>::get_source;
  void run_internal() {
    bool closed = false;
    while(!closed) {
      get_source()->wait();
      closed = get_source()->is_closed();
      if (!closed) {
        consumer_(get_source()->get());
      }
    }
    if (n_threads_ > 0) {
      end_thread();
    } else {
      end_fn_();
    }
  }

  // Invoked when then pipeline has finished, unless a custom end_fn
  // has been set. Counts down the end_latch, so that a thread blocked
  // in wait() can return.
  void default_end() {
    end_latch_.count_down();
  }

  void set_default_end_fn() {
    end_fn_ = bind(&RunnablePipeline::default_end, this);
    default_end_ = true;
  }

  void end_thread() {
    int left = --count_;
    if (left == 0) {
      end_fn_();
    }
  }

  const int n_threads_;
  function <void(IN input)> consumer_;

  function <void()> end_fn_;
  bool default_end_;
  countdown_latch end_latch_;
  atomic_int count_;
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
