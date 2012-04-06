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

#include "blockable_thread.h"
#include "test_mutex.h"

ThreadBlocker::ThreadBlocker() : count_(1), block_latch_(1), start_latch_(1) {
}

ThreadBlocker::ThreadBlocker(int count)
  : count_(count),
    block_latch_(1),
    start_latch_(1) {
}

void ThreadBlocker::SetThread(thread::id id) {
  // Verify that id is null. A ThreadBlocker can only be associated
  // with one thread.
  assert(id_ == thread::id());
  id_ = id;
}

void ThreadBlocker::Block() {
  THREAD_DBG << "ThreadBlocker::Block " << id_ << " count = " << count_ << ENDL;
  if (this_thread::get_id() == id_) {
    if (--count_ == 0) {
      THREAD_DBG << "waiting on latch" << ENDL;
      block_latch_.wait();
    }
  }
}

void ThreadBlocker::Unlock() {
  block_latch_.count_down();
}

void ThreadBlocker::WaitToStart() {
  start_latch_.wait();
}

void ThreadBlocker::Start() {
  start_latch_.count_down();
}
