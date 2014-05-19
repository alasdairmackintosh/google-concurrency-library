// Copyright 2010 Google Inc. All Rights Reserved.
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

#include <thread.h>

#include "barrier.h"

namespace gcl {
using std::atomic;
using std::memory_order;
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
#else
using std::tr1::bind;
#endif

barrier::barrier(int num_threads) throw (std::invalid_argument)
    : thread_count_(num_threads),
      num_waiting_(0) {
  if (num_threads < 0) {
    throw std::invalid_argument("num_threads is negative");
  }
  std::atomic_init(&num_to_leave_, 0);
}

barrier::~barrier() {
  while (!all_threads_exited()) {
    // Don't destroy this object if threads have not yet exited
    // arrive_and_drop(). This can occur when a thread calls
    // arrive_and_drop() followed by the destructor - the waiting threads
    // may be scheduled to wake up, but not yet have exited.
    //
    // NOTE - on pthread systems, could add a yield call here
  }
}

// These methods could be implemented as C++11 lambdas, but are written as
// member functions for C++98 compatibility.
bool barrier::all_threads_exited() {
  return num_to_leave_ == 0;
}

bool barrier::all_threads_waiting() {
  return num_waiting_ == thread_count_;
}

void barrier::arrive_and_wait() {
  {
    unique_lock<mutex> lock(mutex_);
    // TODO(alasdair): This can deadlock - fix it.
    idle_.wait(lock, bind(&barrier::all_threads_exited, this));
    ++num_waiting_;
    if (num_waiting_ == thread_count_) {
      num_to_leave_ = thread_count_;
      ready_.notify_all();
    } else {
      ready_.wait(lock, bind(&barrier::all_threads_waiting, this));
    }
    if (num_to_leave_ == 1) {
      num_waiting_ = 0;
      idle_.notify_all();
    }
  }
  --num_to_leave_;
}

void barrier::arrive_and_drop() {
  {
    unique_lock<mutex> lock(mutex_);
    idle_.wait(lock, bind(&barrier::all_threads_exited, this));
    if (--thread_count_ == 0) {
      throw std::invalid_argument("All threads have left");
    }
    if (num_waiting_ == thread_count_) {
      num_to_leave_ = thread_count_;
      ready_.notify_all();
    }
  }
}

#ifdef HAS_CXX11_RVREF
scoped_guard barrier::arrive_and_wait_guard() {
  function<void ()> f = bind(&barrier::arrive_and_wait, this);
  return scoped_guard(f);
}
#endif
}  // End namespace gcl
