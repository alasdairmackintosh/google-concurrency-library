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

#include "latch.h"

#include "mutex.h"
#include "condition_variable.h"

namespace gcl {
latch::latch(size_t count)
    : count_(count) {
}

void latch::wait() {
  unique_lock<mutex> lock(condition_mutex_);
  while(count_ > 0) {
    condition_.wait(lock);
  }
}

bool latch::try_wait() {
  unique_lock<mutex> lock(condition_mutex_);
  return count_ == 0;
}

void latch::count_down() {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ == 0) {
    throw std::logic_error("internal count == 0");
  }
  if (--count_ == 0) {
    condition_.notify_all();
  }
}

void latch::count_down_and_wait() {
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

}  // End namespace gcl
