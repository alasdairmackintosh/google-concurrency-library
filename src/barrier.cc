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

#include <thread.h>

#include "barrier.h"

namespace gcl {
using std::atomic;
using std::memory_order;
#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
#else
using std::tr1::bind;
#endif

barrier::barrier(size_t num_threads) throw (std::invalid_argument)
    : thread_count_(num_threads),
      new_thread_count_(num_threads),
      num_waiting_(0),
      num_to_leave_(0) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
}

barrier::barrier(size_t num_threads,
                 void (*completion)()) throw (std::invalid_argument)
      : thread_count_(num_threads),
        num_waiting_(0),
        num_to_leave_(0),
        completion_fn_(bind(&barrier::completion_wrapper, this, completion)) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
}

barrier::barrier(size_t num_threads,
                 function<void()> completion) throw (std::invalid_argument)
      : thread_count_(num_threads),
        num_waiting_(0),
        num_to_leave_(0),
        completion_fn_(bind(&barrier::completion_wrapper, this, completion)) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
}

barrier::barrier(
    size_t num_threads,
    function<size_t()> completion) throw (std::invalid_argument)
      : thread_count_(num_threads),
        num_waiting_(0),
        num_to_leave_(0),
        completion_fn_(completion) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
}

barrier::barrier(
    size_t num_threads,
    size_t (*completion)()) throw (std::invalid_argument)
      : thread_count_(num_threads),
        num_waiting_(0),
        num_to_leave_(0),
        completion_fn_(completion) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
}

barrier::~barrier() {
  // TODO(alasdair): The current documentation does not define the destruct
  // behaviour, and this assertion may be too strict. Keeping it for now as it
  // can help with debugging incorrect useage, but we may want to remove it, or
  // replace it with a more complex test.
  unique_lock<mutex> lock(mutex_);
  assert(all_threads_exited());
}

// These methods could be implemented as C++11 lambdas, but are written as
// member functions for C++98 compatibility.
bool barrier::all_threads_exited() {
  return num_to_leave_ == 0;
}

bool barrier::all_threads_waiting() {
  return num_waiting_ == thread_count_;
}

void barrier::count_down_and_wait()  throw (std::logic_error) {
  unique_lock<mutex> lock(mutex_);
  idle_.wait(lock, bind(&barrier::all_threads_exited, this));
  ++num_waiting_;
  if (num_waiting_ == thread_count_) {
    num_to_leave_ = thread_count_;
    on_countdown();
    ready_.notify_all();
  } else {
    ready_.wait(lock, bind(&barrier::all_threads_waiting, this));
  }
  if (--num_to_leave_ == 0) {
    thread_count_ = new_thread_count_;
    num_waiting_ = 0;
    idle_.notify_all();
  }
}

size_t barrier::completion_wrapper(function<void()> completion) {
  completion();
  return thread_count_;
}

void barrier::on_countdown() {
  if (completion_fn_) {
    reset(completion_fn_());
  }
}

void barrier::reset(size_t num_threads) {
  if (num_threads == 0) {
    throw std::invalid_argument("num_threads is 0");
  }
  // TODO(alasdair): Consider adding a check that we are either in the
  // completion function, or have not yet called wait()
  new_thread_count_ = num_threads;
}

}  // End namespace gcl
