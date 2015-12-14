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

#ifndef GCL_FLEX_BARRIER_
#define GCL_FLEX_BARRIER_

#include <stddef.h>
#include <stdexcept>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include "scoped_guard.h"

#include <functional>

namespace gcl {

// Allows a set of threads to wait until all threads have reached a
// common point.
class flex_barrier {
 public:
  template <typename F>
  flex_barrier(std::ptrdiff_t num_threads, F completion) throw (std::invalid_argument);

  explicit flex_barrier(ptrdiff_t num_threads);

  ~flex_barrier();

  flex_barrier(const flex_barrier&) = delete;
  flex_barrier& operator=(const flex_barrier&) = delete;

  void arrive_and_wait();
  void arrive_and_drop();

 private:
  std::ptrdiff_t completion_wrapper(std::function<void()> completion);
  void reset(std::ptrdiff_t num_threads);
  bool all_threads_exited();
  bool all_threads_waiting();
  void on_countdown();

  std::ptrdiff_t thread_count_;
  std::ptrdiff_t new_thread_count_;

  std::mutex mutex_;
  std::condition_variable idle_;
  std::condition_variable ready_;
  std::ptrdiff_t num_waiting_;
  std::atomic<std::ptrdiff_t> num_to_leave_;

  std::function<std::ptrdiff_t()> completion_fn_;
};

template <typename F>
flex_barrier::flex_barrier(std::ptrdiff_t num_threads,
                           F completion) throw(std::invalid_argument)
    : thread_count_(num_threads), num_waiting_(0), completion_fn_(completion) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  num_to_leave_ = 0; //std::atomic_init(&num_to_leave_, 0);
}

}
#endif // GCL_FLEX_BARRIER_
