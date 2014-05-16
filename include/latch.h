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
#include "atomic.h"
#include "condition_variable.h"
#include "mutex.h"
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
  explicit latch(int count);

  ~latch();

  void arrive();

  void arrive_and_wait();

  void count_down(int n);

  void wait();

  bool try_wait();

#ifdef HAS_CXX11_RVREF
  // Creates a scoped_guard that will invoke arrive, wait, or
  // arrive_and_wait on this latch when it goes out of scope.
  scoped_guard arrive_guard();
  scoped_guard wait_guard();
  scoped_guard arrive_and_wait_guard();
#endif

private:
  // The counter for this latch.
  int count_;

  // Counts the number of threads that are currently waiting
  std::atomic_int waiting_;

  // The condition that blocks until the count reaches 0
  condition_variable condition_;
  mutex condition_mutex_;

 // Disallow copy and assign
  latch(const latch&) CXX11_DELETED
  latch& operator=(const latch&) CXX11_DELETED
};

}  // End namespace gcl

#endif  // GCL_LATCH_
