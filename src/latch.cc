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
 : base_(count) {
}

void latch::wait() {
  base_.wait();
}

void latch::count_down() {
  base_.count_down();
}

void latch::count_down_and_wait() {
  base_.count_down_and_wait();
}

}  // End namespace gcl
