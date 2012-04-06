// Copyright 2010 Google Inc. All Rights Reserved.

// Basic test of the serial_executor class.
// Tests that execution works with multiple enqueing threads.

#include <stdio.h>

#include "atomic.h"
#include "condition_variable.h"
#include "serial_executor.h"
#include "thread.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

using std::atomic_int;
using std::memory_order_relaxed;

using gcl::serial_executor;

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

TEST(SerialExecutorTest, SingleExecution) {
  Called called(1);
  serial_executor exec;
  exec.execute(tr1::bind(&Called::run, &called));
  called.wait();

  EXPECT_EQ(1, called.count);
}

TEST(SerialExecutorTest, MultiExecution) {
  const int num_exec = 10;
  Called called(num_exec);

  serial_executor exec;
  // Queue up a number of executions.
  for (int i = 0; i < num_exec; ++i) {
    exec.execute(tr1::bind(&Called::run, &called));
  }
  called.wait();

  EXPECT_EQ(num_exec, called.count);
}

struct LockedTask {
  LockedTask(mutex* lock) : block_lock(lock) {}

  void run() {
    unique_lock<mutex> ul(*block_lock);
    ++count;
  }

  mutex* block_lock;
  int count;
};

void delete_executor(serial_executor* exec) {
  delete exec;
}

TEST(SerialExecutorTest, ShutdownTest) {
  // To test shutting down, we need to create a thread which blocks then kill
  // the executor and try to enqueue more stuff, yet make sure that the
  // executor thread doesn't actually execute the extra stuff.
  mutex task_blocker;
  task_blocker.lock();

  LockedTask locked_task(&task_blocker);

  serial_executor* exec = new serial_executor();
  exec->execute(tr1::bind(&LockedTask::run, &locked_task));
  // NOTE: this should deadlock! Need to start a new thread to handle shutdown
  // since the executor will block waiting for the running task to join!
  thread exec_deleter(tr1::bind(&delete_executor, exec));

  // Let the deleter make some progress.
  this_thread::sleep_for(chrono::milliseconds(100));

  // other_task should never execute.
  Called other_task(1);
  exec->execute(tr1::bind(&Called::run, &other_task));
  task_blocker.unlock();

  // Sleep a bit to give the task a chance to run.
  this_thread::sleep_for(chrono::milliseconds(10));
  exec_deleter.join();

  EXPECT_EQ(0, other_task.count);
}

struct CountMaker {
  CountMaker() : next1(1), next2(2) {}

  void run(serial_executor* executor) {
    executor->execute(tr1::bind(&Called::run, &next2));
    executor->execute(tr1::bind(&Called::run, &next1));
    executor->execute(tr1::bind(&Called::run, &next2));
  }

  Called next1;
  Called next2;
};

TEST(SerialExecutorTest, InlineExecutes) {
  CountMaker inlined_counters;
  serial_executor exec;
  exec.execute(tr1::bind(&CountMaker::run, &inlined_counters, &exec));

  inlined_counters.next1.wait();
  inlined_counters.next2.wait();

  // Check that the spawned executions run as well.
  EXPECT_EQ(1, inlined_counters.next1.count);
  EXPECT_EQ(2, inlined_counters.next2.count);
}
