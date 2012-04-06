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

namespace gcl {
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
#else
using std::tr1::bind;
#endif

barrier::barrier(size_t num_threads) throw (std::invalid_argument)
    : thread_count_(num_threads),
      latch1_(num_threads),
      latch2_(num_threads),
      current_latch_(&latch1_),
      completion_fn_(NULL) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  latch1_.reset(bind(&barrier::on_countdown, this));
  latch2_.reset(bind(&barrier::on_countdown, this));
}

barrier::barrier(size_t num_threads,
                 function<void()> completion_fn) throw (std::invalid_argument)
    : thread_count_(num_threads),
      latch1_(num_threads),
      latch2_(num_threads),
      current_latch_(&latch1_),
      completion_fn_(completion_fn) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  if (completion_fn == NULL) {
    throw std::invalid_argument("completion_fn is NULL");
  }
  latch1_.reset(bind(&barrier::on_countdown, this));
  latch2_.reset(bind(&barrier::on_countdown, this));
}

barrier::~barrier() {
}

void barrier::count_down_and_wait()  throw (std::logic_error) {
  current_latch_->count_down_and_wait();
}

void barrier::on_countdown() {
  current_latch_ = (current_latch_ == &latch1_ ? &latch2_ : &latch1_);
  if (completion_fn_ != NULL) {
    completion_fn_();
  }
}

void barrier::reset(size_t num_threads) {
  // TODO(alasdair): Consider adding a check that we are either in the
  // completion function, or have not yet called wait()
  current_latch_->reset(num_threads);
}

void barrier::reset(function<void()> completion_fn) {
  // TODO(alasdair): Consider adding a check that we are either in the
  // completion function, or have not yet called wait()
  completion_fn_ = completion_fn;
}

}  // End namespace gcl
