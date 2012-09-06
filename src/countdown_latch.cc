// Copyright 2009 Google Inc. All Rights Reserved.
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

#include "mutex.h"
#include "condition_variable.h"

#include "countdown_latch.h"

namespace gcl {

countdown_latch::countdown_latch(unsigned int count)
 : count_(count) {
}

countdown_latch::~countdown_latch() {
  // TODO(alasdair): The documentation says that calling this when other threads
  // are blocked in wait is 'undefined'. Strictly speaking this assertion may
  // fire even if other threads are not actually in wait. Keeping it for now as
  // it can help with debugging incorrect useage, but we may want to remove it,
  // or replace it with a more complex test.
  unique_lock<mutex> lock(condition_mutex_);
  assert(count_ == 0);
}

void countdown_latch::wait() {
  unique_lock<mutex> lock(condition_mutex_);
  while(count_ > 0) {
    condition_.wait(lock);
  }
}

void countdown_latch::count_down() {
  lock_guard<mutex> lock(condition_mutex_);
  if (count_ <= 0) {
    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
  }
  if (--count_ == 0) {
    condition_.notify_all();
  }
}

}  // End namespace gcl
