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
  // Normally we expect count_ == 0, but in some cases a user might create a
  // countdown_latch but abandon it later.  To allow that abandonment we don't
  // check count here.
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
