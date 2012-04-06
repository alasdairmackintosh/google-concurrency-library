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

#include "latch_base.h"

#include "mutex.h"
#include "condition_variable.h"

namespace gcl {

latch_base::latch_base(size_t count)
    : count_(count), function_(NULL) {
}

latch_base::latch_base(size_t count, tr1::function<void()> function)
    : count_(count), function_(function) {
}

void latch_base::wait() {
  unique_lock<mutex> lock(condition_mutex_);
  while(count_ > 0) {
    condition_.wait(lock);
  }
}

void latch_base::count_down(tr1::function<void()> function) {
  lock_guard<mutex> lock(condition_mutex_);
  function_ = function;
  count_down_with_cb();
}

void latch_base::count_down() {
  lock_guard<mutex> lock(condition_mutex_);
  count_down_with_cb();
}

void latch_base::count_down_with_cb() {
  if (count_ == 0) {
    throw std::logic_error("internal count == 0");
  }
  if (--count_ == 0) {
    if (function_ != NULL) {
      function_();
    }
    condition_.notify_all();
  }
}

void latch_base::count_down_and_wait(tr1::function<void()> function) {
  unique_lock<mutex> lock(condition_mutex_);
  function_ = function;
  count_down_and_wait_with_cb(lock);
}

void latch_base::count_down_and_wait() {
  unique_lock<mutex> lock(condition_mutex_);
  count_down_and_wait_with_cb(lock);
}

void latch_base::count_down_and_wait_with_cb(unique_lock<mutex>& lock) {
  if (count_ == 0) {
    throw std::logic_error("internal count == 0");
  }
  if (--count_ == 0) {
    if (function_ != NULL) {
      function_();
    }
    condition_.notify_all();
  } else {
    while(count_ > 0) {
      condition_.wait(lock);
    }
  }
}

void latch_base::reset(size_t count) {
  count_ = count;
}

}  // End namespace gcl
