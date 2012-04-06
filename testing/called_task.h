// Copyright 2011 Google Inc. All Rights Reserved.

#include "atomic.h"
#include "condition_variable.h"
#include "mutable_thread.h"

using std::atomic_int;
using std::memory_order_relaxed;

namespace gcl {

// Helper class which executes a basic add function and blocks the thread until
// the ready-count of threads is hit (this is very much like a barrier-closure
// in behavior but with a little bit more test-functionality).
struct Called {
  Called(int ready_count) : ready_count(ready_count) {
    atomic_init(&count, 0);
  }

  void run() {
    unique_lock<mutex> wait_lock(ready_lock);
    count.fetch_add(1, memory_order_relaxed);
    ready_condvar.notify_one();
  }

  // Blocking wait function which returns when count reaches ready_count
  void wait() {
    unique_lock<mutex> wait_lock(ready_lock);
    ready_condvar.wait(wait_lock, tr1::bind(&Called::is_done, this));
  }

  atomic_int count;

  int ready_count;
  mutex ready_lock;
  condition_variable ready_condvar;

 private:
  bool is_done() {
    return ready_count == count.load(memory_order_relaxed);
  }
};

}  // namespace gcl
