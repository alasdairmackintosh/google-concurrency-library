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
    : count_(count) {
}

latch_base::latch_base(size_t count, function<void()> function)
    : count_(count), completion_fn_(function) {
}

void latch_base::wait() {
  unique_lock<mutex> lock(condition_mutex_);
  while(count_ > 0) {
    condition_.wait(lock);
  }
}

void latch_base::count_down() {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ == 0) {
    throw std::logic_error("internal count == 0");
  }
  if (--count_ == 0) {
    if (completion_fn_) {
      completion_fn_();
    }
    condition_.notify_all();
  }
}

void latch_base::count_down_and_wait() {
  unique_lock<mutex> lock(condition_mutex_);
  if (count_ == 0) {
    throw std::logic_error("internal count == 0");
  }
  if (--count_ == 0) {
    if (completion_fn_) {
      completion_fn_();
    }
    condition_.notify_all();
  } else {
    while(count_ > 0) {
      condition_.wait(lock);
    }
  }
}

bool latch_base::count_up() {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ == 0) {
    return false;
  }
  ++count_;
  return true;
}

void latch_base::reset(size_t count) {
  lock_guard<mutex> lock(condition_mutex_);
  count_ = count;
}

void latch_base::reset(function<void()> completion_fn) {
  lock_guard<mutex> lock(condition_mutex_);
  completion_fn_ = completion_fn;
}

}  // End namespace gcl
