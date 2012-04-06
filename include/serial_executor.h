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

#ifndef GCL_SERIAL_EXECUTOR_
#define GCL_SERIAL_EXECUTOR_

#include <queue>

#include "functional.h"

#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

namespace gcl {

// Simple executor which creates a new thread for controlling and executing
// parameter-free function objects.

// TODO(mysen): The simple interface should allow the caller to indicate which
// queueing mechanism to use to select objects to run.
class serial_executor {
 public:
  // Standard executor interface which creates a FIFO queue of objects to
  // execute.
  serial_executor();
  ~serial_executor();

  // Simple execute command to execute a function at a convenient time.
  // Copies the contents of the function object and runs in the executor thread.
  void execute(std::function<void()> fn);

 private:
  // Queue of functions to execute.
  std::queue<std::function<void()> > function_queue;

  // Bool indicating that the class is in a state of shut-down. Used to wake up
  // the run thread and and finish execution.
  bool shutting_down;

  // Lock to serialize accesses to the function queue.
  mutex queue_lock;
  condition_variable queue_condvar;

  // Thread used by the executor to do all its work.
  thread run_thread;

  // Internal run function which handles all the execution logic.
  void run();

  // Function which indicates that a job is ready to execute.
  // Does not do any locking, so it assumes that the caller will take ownership
  // of the queue_lock var before calling this.
  bool queue_ready();
};

}  // End namespace gcl

#endif  // GCL_SERIAL_EXECUTOR_
