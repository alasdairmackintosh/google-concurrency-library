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

#include "functional.h"

#include "mutex.h"
#include "condition_variable.h"

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
  barrier(size_t num_threads, std::function<void()> func)
      throw (std::invalid_argument);

  // Blocks until num_threads have call await(). Invokes the function
  // specified in the constructor before releasing any thread. Returns
  // true to one of the callers, and false to the others, allowing one
  // caller to delete the barrier.
  //
  // Memory ordering: For threads X and Y that call await(), the call
  // to await() in X happens before the return from await() in Y.
  //
  // A logic_error will be thrown if more than num_threads call await().
  bool await() throw (std::logic_error);

 private:
  mutex lock_;
  size_t num_to_block_left_;
  size_t num_to_exit_;
  condition_variable cv_;
  std::function<void()> function_;
};
}
#endif // GCL_BARRIER_
