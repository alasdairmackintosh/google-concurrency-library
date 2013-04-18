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

#ifndef GCL_BARRIER_
#define GCL_BARRIER_

#include <stddef.h>
#include <stdexcept>

#include <atomic.h>
#include <condition_variable.h>
#include <mutex.h>

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
#include <functional>
#else
#include <tr1/functional>
#endif

namespace gcl {
using std::atomic;
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
using std::function;
#else
using std::tr1::function;
using std::tr1::bind;
#endif

// Allows a set of threads to wait until all threads have reached a
// common point.
class barrier {
 public:
  // Creates a new barrier that will block until num_threads threads are waiting
  // on it. When the barrier is released, it will be reset, so that it can be
  // used again.
  // Throws invalid_argument if num_threads == 0.
  explicit barrier(size_t num_threads) throw (std::invalid_argument);

  // Creates a new barrier that will block until num_threads threads are waiting
  // on it. When the barrier is released, completion will be invoked, and the
  // barrier will be reset. If the completion is NULL, no function will be
  // invoked upon completion.
  // Throws invalid_argument if num_threads == 0.
  barrier(size_t num_threads, function<void()> completion) throw (std::invalid_argument);
  barrier(size_t num_threads, void (*completion)()) throw (std::invalid_argument);

  // Creates a new barrier that will block until num_threads threads are waiting
  // on it. When the barrier is released, completion will be invoked, and the
  // barrier will be reset to the number of threads returned by completion. If
  // the completion is NULL, no function will be invoked upon completion.
  // Throws invalid_argument if num_threads == 0.
  barrier(size_t num_threads, function<size_t()> completion) throw (std::invalid_argument);
  barrier(size_t num_threads, size_t (*completion)()) throw (std::invalid_argument);

  ~barrier();

  // Blocks until num_threads have call count_down_and_wait(). Before releasing
  // any thread, invokes the completion function specified in the constructor.
  // If no completion function is registered, resets itself with the original
  // num_threads count.
  //
  // Memory ordering: For threads X and Y that call count_down_and_wait(), the
  // call to count_down_and_wait() in X happens before the return from
  // count_down_and_wait() in Y.
  void count_down_and_wait() throw (std::logic_error);

 private:
  size_t completion_wrapper(function<void()> completion);
  void reset(size_t num_threads);
  bool all_threads_exited();
  bool all_threads_waiting();
  void on_countdown();

  size_t thread_count_;
  size_t new_thread_count_;

  mutex mutex_;
  condition_variable idle_;
  condition_variable ready_;
  size_t num_waiting_;
  size_t num_to_leave_;

  function<size_t()> completion_fn_;
};
}
#endif // GCL_BARRIER_
