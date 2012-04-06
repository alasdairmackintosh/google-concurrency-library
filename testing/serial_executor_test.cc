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

// Basic test of the serial_executor class.
// Tests that execution works with multiple enqueing threads.

#include <stdio.h>

#include "functional.h"

#include "atomic.h"

#include "condition_variable.h"
#include "thread.h"

#include "called_task.h"
#include "serial_executor.h"

#include "gmock/gmock.h"

namespace gcl {
namespace {

TEST(SerialExecutorTest, SingleExecution) {
  Called called(1);
  serial_executor exec;
  exec.execute(std::bind(&Called::run, &called));
  called.wait();

  EXPECT_EQ(1, called.count);
}

TEST(SerialExecutorTest, MultiExecution) {
  const int num_exec = 10;
  Called called(num_exec);

  serial_executor exec;
  // Queue up a number of executions.
  for (int i = 0; i < num_exec; ++i) {
    exec.execute(std::bind(&Called::run, &called));
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
  exec->execute(std::bind(&LockedTask::run, &locked_task));
  // Let the executor thread make some progress.
  this_thread::sleep_for(chrono::milliseconds(100));

  // NOTE: this should deadlock! Need to start a new thread to handle shutdown
  // since the executor will block waiting for the running task to join!
  thread exec_deleter(std::bind(&delete_executor, exec));

  // Let the deleter make some progress.
  this_thread::sleep_for(chrono::milliseconds(100));

  // other_task should never execute.
  Called other_task(1);
  exec->execute(std::bind(&Called::run, &other_task));
  task_blocker.unlock();

  // Sleep a bit to give the task a chance to run.
  this_thread::sleep_for(chrono::milliseconds(10));
  exec_deleter.join();

  EXPECT_EQ(0, other_task.count);
}

struct CountMaker {
  CountMaker() : next1(1), next2(2) {}

  void run(serial_executor* executor) {
    executor->execute(std::bind(&Called::run, &next2));
    executor->execute(std::bind(&Called::run, &next1));
    executor->execute(std::bind(&Called::run, &next2));
  }

  Called next1;
  Called next2;
};

TEST(SerialExecutorTest, InlineExecutes) {
  CountMaker inlined_counters;
  serial_executor exec;
  exec.execute(std::bind(&CountMaker::run, &inlined_counters, &exec));

  inlined_counters.next1.wait();
  inlined_counters.next2.wait();

  // Check that the spawned executions run as well.
  EXPECT_EQ(1, inlined_counters.next1.count);
  EXPECT_EQ(2, inlined_counters.next2.count);
}

}
}  // namespace gcl
