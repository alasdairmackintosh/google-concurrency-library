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

#include <thread_barrier.h>

namespace gcl {
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
#else
using std::tr1::bind;
#endif

thread_barrier::thread_barrier(size_t num_threads) throw (std::invalid_argument)
    : thread_count_(num_threads),
      latch1_(num_threads),
      latch2_(num_threads),
      current_latch_(&latch1_) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  latch1_.reset(bind(&thread_barrier::on_countdown, this));
  latch2_.reset(bind(&thread_barrier::on_countdown, this));
}


thread_barrier::~thread_barrier() {
}

void thread_barrier::count_down_and_wait()  throw (std::logic_error) {
  current_latch_->count_down_and_wait();
}

void thread_barrier::on_countdown() {
  current_latch_ = (current_latch_ == &latch1_ ? &latch2_ : &latch1_);
  current_latch_->reset(thread_count_);
  if (completion_fn_) {
    completion_fn_();
  }
}

}  // End namespace gcl
