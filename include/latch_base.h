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

namespace tr1 = std::tr1;

namespace gcl {

// A latch allows one or more threads to block until an operation is
// completed. A latch is initialized with a count value. Calls to count_down()
// will decrement this count. Calls to wait() will block until the count
// reaches zero. All calls to count_down() happen before any call to wait()
// returns.
class latch_base {
public:

  // Creates a new latch with the given count.
  explicit latch_base(size_t count);

  // Creates a new latch with the given count, and a function to be invoked when
  // the count reaches 0
  explicit latch_base(size_t count, tr1::function<void()> function);

  // Destroys the latch. If the latch is destroyed while other threads are
  // blocked in wait(), or are invoking count_down(), the behaviour is
  // undefined. Note that a single waiter can safely destroy the latch as soon
  // as it returns from wait().
  ~latch_base() {
  }

  // Decrements the count, and returns. If the count reaches 0, any threads
  // blocked in wait() will be released.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down();

  // Decrements the count and returns. If the count reaches 0, then 'function'
  // will be invoked before any other threads blocked in wait are released. If
  // more than one thread invokes this method with different values of
  // 'function', then there is no guarantee as to which function will be
  // invoked.
  void count_down(tr1::function<void()> function);

  // Waits until the count is decremented to 0. If the count is already 0, this
  // is a no-op.
  void wait();

  // Decrements the count, and waits until it reaches 0. When it does, this
  // thread, as well as any oher threads blocked in wait() will be released.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down_and_wait();

  // Decrements the count, and waits until it reaches 0. When it does, this
  // thread, as well as any oher threads blocked in wait() will be released.
  // Before any thread is released, 'function' will be invoked. If more than one
  // thread invokes this method with different values of 'function', then there
  // is no guarantee as to which function will be invoked.
  //
  // Throws std::logic_error if the internal count is already 0.
  void count_down_and_wait(tr1::function<void()> function);

  // Resets the latch with a new count value. The only safe place to call this
  // method is from the function invoked from count_down.
  void reset(size_t new_count);

private:
  void count_down_with_cb();
  void count_down_and_wait_with_cb(unique_lock<mutex>& lock);

  // The counter for this latch.
  size_t count_;

  // The condition that blocks until the count reaches 0
  condition_variable condition_;
  mutex condition_mutex_;

  // The function to be invoked on count_down
  tr1::function<void()> function_;

  // Disallow copy and assign
  latch_base(const latch_base&) CXX0X_DELETED
  latch_base& operator=(const latch_base&) CXX0X_DELETED
};

}  // End namespace gcl

#endif  // GCL_LATCH_BASE_
