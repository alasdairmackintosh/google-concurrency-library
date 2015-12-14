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

#ifndef GCL_LATCH_
#define GCL_LATCH_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include "scoped_guard.h"

namespace gcl {

// A latch allows one or more threads to block until an operation is
// completed. A latch is initialized with a count value. Calls to count_down()
// will decrement this count. Calls to wait() will block until the count
// reaches zero. All calls to count_down() happen before any call to wait()
// returns.
class latch {
public:
  // Creates a new latch with the given count.
  explicit latch(std::ptrdiff_t count);

  ~latch();

  latch(const latch&) = delete;
  latch& operator=(const latch&) = delete;

  void count_down_and_wait();

  void count_down(std::ptrdiff_t n = 1);

  bool is_ready() const noexcept;

  void wait() const;

private:
  // The counter for this latch.
  std::ptrdiff_t count_;

  // Counts the number of threads that are currently waiting
  mutable std::atomic<std::ptrdiff_t> waiting_;

  // The condition that blocks until the count reaches 0
  mutable std::condition_variable condition_;
  mutable std::mutex condition_mutex_;
};

}  // End namespace gcl

#endif  // GCL_LATCH_
