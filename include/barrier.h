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
#include <tr1/functional>

#include <condition_variable.h>
#include <latch_base.h>
#include <mutex.h>

namespace tr1 = std::tr1;

namespace gcl {

// Allows a set of threads to wait until all threads have reached a
// common point.
class barrier {
 public:
  // Creates a new barrier that will block until num_threads threads
  // are waiting on it. Throws invalid_argument if num_threads == 0.
  explicit barrier(size_t num_threads) throw (std::invalid_argument);

  // Creates a new barrier that will block until num_threads threads
  // are waiting on it. When the barrier is released, function will
  // be invoked. Throws invalid_argument if num_threads == 0.
  barrier(size_t num_threads, tr1::function<void()> function)
      throw (std::invalid_argument);

  ~barrier();

  // Blocks until num_threads have call await(). Invokes the function
  // specified in the constructor before releasing any thread. Throws
  // an error if more than num_threads call this method.
  //
  // Once all threads have returned from await(), the barrier resets itself
  // with the same num_threads count, and can be reused. The re-use is
  // thread-safe, and a thread that returns from await() can immediately call
  // await() again.
  //
  // Memory ordering: For threads X and Y that call await(), the call
  // to await() in X happens before the return from await() in Y.
  void await() throw (std::logic_error);

  // Sets the number of threads that can wait on this barrier. This should only
  // be invoked from within the callback function passed at creation time
  void set_num_threads(size_t num_threads);

 private:
  void wait_function();

  latch_base latch_;
  tr1::function<void()> function_;
};
}
#endif // GCL_BARRIER_
