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

#include "atomic.h"
#include "condition_variable.h"

#include "mutable_thread.h"

using std::atomic;
using std::memory_order_relaxed;

namespace gcl {

// Helper class which executes a basic add function and blocks the thread until
// the ready-count of threads is hit (this is very much like a barrier-closure
// in behavior but with a little bit more test-functionality).
struct Called {
  Called(int ready_count) {
    std::atomic_init(&this->ready_count, ready_count);
    std::atomic_init(&count, 0);
  }

  void run() {
    unique_lock<mutex> wait_lock(ready_lock);
    count.fetch_add(1, memory_order_relaxed);
    ready_condvar.notify_one();
  }

  // Blocking wait function which returns when count reaches ready_count
  void wait() {
    unique_lock<mutex> wait_lock(ready_lock);
    ready_condvar.wait(wait_lock, std::bind(&Called::is_done, this));
  }

  void update_count(int new_ready_count) {
    int expected = 2;
    while (!std::atomic_compare_exchange_weak(&ready_count,
                                              &expected,
                                              new_ready_count)) {}
  }

  atomic<int> count;

  atomic<int> ready_count;
  mutex ready_lock;
  condition_variable ready_condvar;

 private:
  bool is_done() {
    return ready_count.load() == count.load(memory_order_relaxed);
  }
};

}  // namespace gcl
