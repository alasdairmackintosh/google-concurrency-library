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

#ifndef GCL_THREAD_BARRIER_
#define GCL_THREAD_BARRIER_

#include <stddef.h>
#include <stdexcept>

#include <condition_variable.h>
#include <latch_base.h>
#include <mutex.h>

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
#include <functional>
#else
#include <tr1/functional>
#endif

namespace gcl {
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::function;
using std::bind;
#else
using std::tr1::function;
using std::tr1::bind;
#endif

// Allows a set of threads to wait until all threads have reached a common
// point. Once this occurs, the thread_barrier will reset itself, and each
// thread will continue to run.
//
// The thread_barrier is designed to be used with a fixed set of threads.
class thread_barrier {
 public:
  // Creates a new thread_barrier that will block until num_threads threads are waiting
  // on it. When the thread_barrier is released, it will be reset, so that it can be
  // used again.
  // Throws invalid_argument if num_threads == 0.
  explicit thread_barrier(size_t num_threads) throw (std::invalid_argument);

  // Creates a new thread_barrier that will block until num_threads threads are
  // waiting on it. When the thread_barrier is released, completion_fn will be
  // invoked. If the completion is NULL, no function will be invoked upon
  // completion.
  // Throws invalid_argument if num_threads == 0.
  template <typename C>
  thread_barrier(size_t num_threads, C completion)
      throw (std::invalid_argument)
      : thread_count_(num_threads),
        latch1_(num_threads),
        latch2_(num_threads),
        current_latch_(&latch1_),
        completion_fn_(completion) {
    if (num_threads == 0) {
      throw std::invalid_argument("num_threads is 0");
    }
    latch1_.reset(bind(&thread_barrier::on_countdown, this));
    latch2_.reset(bind(&thread_barrier::on_countdown, this));
  }


  ~thread_barrier();

  // Blocks until num_threads have call count_down_and_wait(). Before releasing
  // any thread, invokes the completion function specified in the constructor,
  // if any. Resets itself with the original num_threads count.
  //
  // Memory ordering: For threads X and Y that call await(), the call
  // to await() in X happens before the return from await() in Y.
  void count_down_and_wait() throw (std::logic_error);

  // Resets the thread_barrier with the specified completion_fn.  This method should
  // only be invoked when there are no other threads currently inside the wait()
  // method. It is also safe to invoke this method from within the registered
  // completion_fn.
  //
  // If completion_fn is NULL, then the thread_barrier behaves as if no completion_fn
  // were registered in the constructor.
  template <typename C>
  void reset(C completion) {
    completion_fn_ = completion;
  }

 private:
  void on_countdown();

  const size_t thread_count_;
  latch_base latch1_;
  latch_base latch2_;
  latch_base* current_latch_;
  function<void()> completion_fn_;
};
}
#endif // GCL_THREAD_BARRIER_
