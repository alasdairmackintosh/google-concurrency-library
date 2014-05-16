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
#include <iostream>


#include "latch.h"

#include "mutex.h"
#include "condition_variable.h"

namespace gcl {
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
using std::function;
#else
using std::tr1::bind;
using std::tr1::function;
#endif

latch::latch(int count)
    : count_(count) {
  std::atomic_init(&waiting_, 0);
}

latch::~latch() {
  while (waiting_ > 0) {
    // Don't destroy this object if threads have not yet exited wait(). This can
    // occur when a thread calls count_down() followed by the destructor - the
    // waiting threads may be scheduled to wake up, but have not yet have exited.
    //
    // NOTE - on pthread systems, could add a yield call here
  }
}

void latch::wait() {
  ++waiting_;
  {
    unique_lock<mutex> lock(condition_mutex_);
    while(count_ > 0) {
      condition_.wait(lock);
    }
  }
  --waiting_;
}

bool latch::try_wait() {
  ++waiting_;
  bool success;
  {
    unique_lock<mutex> lock(condition_mutex_);
    success = (count_ == 0);
  }
  --waiting_;
  return success;
}

void latch::count_down(int n) {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ - n < 0) {
    throw std::logic_error("internal count == 0");
  }
  count_ -= n;
  if (count_ == 0) {
    condition_.notify_all();
  }
}

void latch::arrive() {
  count_down(1);
}

void latch::arrive_and_wait() {
  ++waiting_;
  {
    unique_lock<mutex> lock(condition_mutex_);
    if (count_ == 0) {
      throw std::logic_error("internal count == 0");
    }
    if (--count_ == 0) {
      condition_.notify_all();
    } else {
      while(count_ > 0) {
        condition_.wait(lock);
      }
    }
  }
  --waiting_;
}

#ifdef HAS_CXX11_RVREF
scoped_guard latch::arrive_guard() {
  function<void ()> f = bind(&latch::arrive, this);
  return scoped_guard(f);
}

scoped_guard latch::wait_guard() {
  function<void ()> f = bind(&latch::wait, this);
  return scoped_guard(f);
}

scoped_guard latch::arrive_and_wait_guard() {
  function<void ()> f = bind(&latch::arrive_and_wait, this);
  return scoped_guard(f);
}
#endif
}  // End namespace gcl
