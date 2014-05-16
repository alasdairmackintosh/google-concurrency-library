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

#ifndef GCL_NOTIFYING_BARRIER_
#define GCL_NOTIFYING_BARRIER_

#include <stddef.h>
#include <stdexcept>

#include <atomic.h>
#include <condition_variable.h>
#include <mutex.h>
#include "scoped_guard.h"

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
class notifying_barrier {
 public:
  template <typename F>
  notifying_barrier(int num_threads, F completion) throw (std::invalid_argument);

  ~notifying_barrier();

  void arrive_and_wait() throw (std::logic_error);

#ifdef HAS_CXX11_RVREF
  // Creates a scoped_guard that will invoke arrive_and_wait on this
  // notifying_barrier when it goes out of scope.
  scoped_guard arrive_and_wait_guard();
#endif

 private:
  int completion_wrapper(function<void()> completion);
  void reset(int num_threads);
  bool all_threads_exited();
  bool all_threads_waiting();
  void on_countdown();

  int thread_count_;
  int new_thread_count_;

  mutex mutex_;
  condition_variable idle_;
  condition_variable ready_;
  int num_waiting_;
  std::atomic_int num_to_leave_;

  function<int()> completion_fn_;
};

template <typename F>
notifying_barrier::notifying_barrier(int num_threads,
                                     F completion) throw(std::invalid_argument)
    : thread_count_(num_threads), num_waiting_(0), completion_fn_(completion) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  std::atomic_init(&num_to_leave_, 0);
}

}
#endif // GCL_NOTIFYING_BARRIER_
