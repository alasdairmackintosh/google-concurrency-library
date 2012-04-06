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

#ifndef GCL_LATCH_BASE_
#define GCL_LATCH_BASE_

#include <tr1/functional>

#include <cxx0x.h>
#include <condition_variable.h>
#include <mutex.h>

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
#include <functional>
#else
#include <tr1/functional>
#endif

namespace gcl {
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::function;
#else
using std::tr1::function;
#endif

// A latch allows one or more threads to block until an operation is
// completed. A latch is initialized with a count value. Calls to count_down()
// will decrement this count. Calls to wait() will block until the count
// reaches zero. All calls to count_down() happen before any call to wait()
// returns.
class latch_base {
public:

  // Creates a new latch with the given count.
  explicit latch_base(size_t count);

  // Creates a new latch with the given count, and a completion function to be
  // invoked when the count reaches 0.
  //
  // Throws invalid_argument if completion_fn is NULL.
  latch_base(size_t count, function<void()> completion_fn);

  // Destroys the latch. If the latch is destroyed while other threads are in
  // wait(), or are invoking count_down(), the behaviour is undefined.
  ~latch_base() {
  }

  // Decrements the count, and returns. If the count reaches 0, any threads
  // blocked in wait() will be released. Before any thread is released, the
  // registered completion_fn will be invoked.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down();

  // Waits until the count is decremented to 0. If the count is already 0, this
  // is a no-op.
  void wait();

  // Decrements the count, and waits until it reaches 0. This is equivalent to
  // calling count_down() followed by wait() as a single atomic operation.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down_and_wait();

  // Increments the current count by one and returns true, unless the count is
  // already at 0, in which case returns false and does nothing.
  bool count_up();

  // Resets the latch with a new count value. This method should only be invoked
  // when there are no other threads currently inside the wait() method. In
  // particular, it is not safe to call this method from the registered
  // completion_fn.
  void reset(size_t new_count);

  // Resets the latch with a new completion_fn. This method should only be
  // invoked when there are no other threads currently inside the wait()
  // method. In particular, it is not safe to call this method from the
  // registered completion_fn.  If completion_fn is NULL, no function will be
  // invoked the next time the count reaches 0.
  void reset(function<void()> completion_fn);

 private:

  // The counter for this latch.
  size_t count_;

  // The condition that blocks until the count reaches 0
  condition_variable condition_;
  mutex condition_mutex_;

  // The function to be invoked on count_down
  function<void()> completion_fn_;

  // Disallow copy and assign
  latch_base(const latch_base&) CXX0X_DELETED
  latch_base& operator=(const latch_base&) CXX0X_DELETED
};

}  // End namespace gcl

#endif  // GCL_LATCH_BASE_
