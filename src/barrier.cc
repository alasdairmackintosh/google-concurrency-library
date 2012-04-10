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

#include "barrier.h"

namespace gcl {

barrier::barrier(size_t num_threads) throw (std::invalid_argument)
    : num_to_block_left_(num_threads),
      num_to_exit_(num_threads),
      function_(NULL) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads must be > 0");
  }
}

barrier::barrier(size_t num_threads, std::function<void()> function)
    throw (std::invalid_argument)
    : num_to_block_left_(num_threads),
      num_to_exit_(num_threads),
      function_(function) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads must be > 0");
  }
}

bool barrier::await()  throw (std::logic_error) {
  unique_lock<mutex> ul(lock_);
  if (num_to_block_left_ == 0) {
    throw std::logic_error("called too many times.");
  }
  if (--num_to_block_left_ > 0) {
    while (num_to_block_left_ != 0) {
      cv_.wait(ul);
    }
  } else {
    if (function_ != NULL) {
      function_();
    }
    cv_.notify_all();
  }
  num_to_exit_--;
  return (num_to_exit_ == 0);
}

}  // End namespace gcl
