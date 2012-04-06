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

#include <barrier.h>
namespace tr1 = std::tr1;

namespace gcl {

barrier::barrier(size_t num_threads) throw (std::invalid_argument)
    : latch_(num_threads, tr1::bind(&barrier::wait_function, this)),
      function_(NULL) {
}

barrier::barrier(size_t num_threads, tr1::function<void()> function)
  throw (std::invalid_argument)
    : latch_(num_threads, tr1::bind(&barrier::wait_function, this)),
      function_(function) {
}

barrier::~barrier() {
}

void barrier::await()  throw (std::logic_error) {
  latch_.count_down_and_wait();
}

void barrier::wait_function() {
  if (function_ != NULL) {
    function_();
  }
}
void barrier::set_num_threads(size_t num_threads) {
  latch_.reset(num_threads);
}
}  // End namespace gcl
