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

#include <queue>

#include "functional.h"

#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

#include "serial_executor.h"

namespace gcl {

serial_executor::serial_executor()
  : shutting_down(false),
    run_thread(std::bind(&serial_executor::run, this)) {}

serial_executor::~serial_executor() {
  // Try to do a safe shutdown of the executor.
  // This will still wait for the currently executing task to complete, but
  // drops any future execution.
  queue_lock.lock();
  shutting_down = true;
  while (!function_queue.empty()) {
    function_queue.pop();
  }

  queue_condvar.notify_one();
  queue_lock.unlock();
  run_thread.join();
}

void serial_executor::execute(std::function<void()> fn) {
  lock_guard<mutex> guard(queue_lock);
  function_queue.push(fn);
  queue_condvar.notify_one();
}

// Nothing fancy in the run interface (except that it is quite wasteful
// of CPU). Probably should move this to use a notification.
void serial_executor::run() {
  while (!shutting_down) {
    unique_lock<mutex> ul(queue_lock);
    queue_condvar.wait(ul, std::bind(&serial_executor::queue_ready, this));

    // Queue can be ready if the executor is shutting down and the runner must
    // finish up to exit.
    if (!shutting_down) {
      std::function<void()> run_function = function_queue.front();
      function_queue.pop();
      ul.unlock();

      run_function();
    }
  }
}

bool serial_executor::queue_ready() {
  return !function_queue.empty() || shutting_down;
}

}  // End namespace gcl
