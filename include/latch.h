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

#include "cxx11.h"
#include "condition_variable.h"
#include "mutex.h"

namespace gcl {

// A latch allows one or more threads to block until an operation is
// completed. A latch is initialized with a count value. Calls to count_down()
// will decrement this count. Calls to wait() will block until the count
// reaches zero. All calls to count_down() happen before any call to wait()
// returns.
class latch {
public:

  // Creates a new latch with the given count.
  explicit latch(size_t count);

  // Destroys the latch. If the latch is destroyed while other threads are in
  // wait(), or are invoking count_down(), the behaviour is undefined.
  ~latch() {
  }

  // Decrements the count, and returns. If the count reaches 0, any threads
  // blocked in wait() will be released.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down();

  // Waits until the count is decremented to 0. If the count is already 0, this
  // is a no-op.
  void wait();

  // Returns true if the count has been decremented to 0, and false
  // otherwise. Does not block.
  bool try_wait();

  // Decrements the count, and waits until it reaches 0. When it does, this
  // thread, as well as any other threads blocked in wait() will be released.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down_and_wait();

private:
  // The counter for this latch.
  size_t count_;

  // The condition that blocks until the count reaches 0
  condition_variable condition_;
  mutex condition_mutex_;

 // Disallow copy and assign
  latch(const latch&) CXX11_DELETED
  latch& operator=(const latch&) CXX11_DELETED
};

}  // End namespace gcl

#endif  // GCL_LATCH_
