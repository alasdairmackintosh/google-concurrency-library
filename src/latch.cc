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

#include <mutex>
#include <condition_variable>

namespace gcl {

latch::latch(std::ptrdiff_t count) : count_(count) {
  waiting_ = 0; // std::atomic_init(&waiting_, 0);
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

void latch::wait() const {
  ++waiting_;
  {
    std::unique_lock<std::mutex> lock(condition_mutex_);
    while(count_ > 0) {
      condition_.wait(lock);
    }
  }
  --waiting_;
}

bool latch::is_ready() const noexcept {
  std::unique_lock<std::mutex> lock(condition_mutex_);
  return (count_ == 0);
}

void latch::count_down(std::ptrdiff_t n) {
  std::lock_guard<std::mutex> lock(condition_mutex_);
  if (count_ - n < 0) {
    throw std::logic_error("internal count == 0");
  }
  count_ -= n;
  if (count_ == 0) {
    condition_.notify_all();
  }
}

void latch::count_down_and_wait() {
  ++waiting_;
  {
    std::unique_lock<std::mutex> lock(condition_mutex_);
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

}  // End namespace gcl
