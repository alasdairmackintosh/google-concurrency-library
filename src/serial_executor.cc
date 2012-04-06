// Copyright 2009 Google Inc. All Rights Reserved.

#include <queue>
#include <tr1/functional>

#include "serial_executor.h"

#include "condition_variable.h"
#include "mutex.h"
#include "thread.h"

namespace tr1 = std::tr1;

serial_executor::serial_executor()
  : shutting_down(false),
    run_thread(tr1::bind(&serial_executor::run, this)) {}

serial_executor::~serial_executor() {
  // Try to do a safe shutdown of the executor.
  // Unfortunately, this will still wait for executions to complete
  // (particularly whatever is currently executing).
  queue_lock.lock();
  shutting_down = true;
  while (!function_queue.empty()) {
    function_queue.pop();
  }

  // Unlock the queue before notifying waiters on the condvar.
  queue_condvar.notify_one();
  queue_lock.unlock();
  run_thread.join();
}

void serial_executor::execute(tr1::function<void()> fn) {
  lock_guard<mutex> guard(queue_lock);
  if (shutting_down) {
    return;
  }

  function_queue.push(fn);
  queue_condvar.notify_one();
}

// Nothing fancy in the run interface (except that it is quite wasteful
// of CPU). Probably should move this to use a notification.
void serial_executor::run() {
  while (!shutting_down) {
    unique_lock<mutex> ul(queue_lock);
    queue_condvar.wait(ul, tr1::bind(&serial_executor::queue_ready, this));

    // Queue can be ready if the executor is shutting down and the runner must
    // finish up to exit.
    if (!shutting_down) {
      tr1::function<void()> run_function = function_queue.front();
      function_queue.pop();
      ul.unlock();

      run_function();
    }
  }
}

bool serial_executor::queue_ready() {
  return !function_queue.empty() || shutting_down;
}
