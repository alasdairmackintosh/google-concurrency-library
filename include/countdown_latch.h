// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef GCL_COUNTDOWN_LATCH_
#define GCL_COUNTDOWN_LATCH_
#include "mutex.h"
#include "condition_variable.h"

namespace gcl {

// A countdown_latch allows one or more threads to block until an
// operation is completed. A countdown_latch is initialized with a
// count value. Calls to count_down() will decrement this count. Calls
// to wait() will block until the count reaches zero. All calls to
// count_down() happen before any call to wait() returns.
class countdown_latch {
public:

  // Creates a new countdown_latch with the given count.
  explicit countdown_latch(unsigned int count);

  // Destroys the countdown_latch. If the latch is destroyed while
  // other threads are blocked in wait(), or are invoking count_down(),
  // the behaviour is undefined. Note that a single waiter can safely
  // destroy the latch as soon as it returns from wait().
  ~countdown_latch();

  // Waits until the count is decremented to 0. If the count is
  // already 0, this is a no-op.
  void wait();

  // Decrements the count. If the count reaches 0, any threads blocked
  // in wait() will be released. If the count is already 0,
  // throws std::system_error.
  void count_down();

private:
  // The counter for this latch.
  unsigned int count_;

  // The condition that blocks until the count reaches 0
  condition_variable condition_;
  mutex condition_mutex_;

 // Disallow copy and assign
  countdown_latch(const countdown_latch&);
  countdown_latch& operator=(const countdown_latch&);
};

}  // End namespace gcl

#endif  // GCL_COUNTDOWN_LATCH_
