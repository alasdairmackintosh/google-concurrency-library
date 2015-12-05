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

#ifndef STD_TEST_MUTEX_
#define STD_TEST_MUTEX_

#include <map>
#include <iostream>

#include "functional.h"

#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

// Prints debug messages. The output is locked to ensure that only
// one thread can print at a time. Prints out the thread id at the
// start of the line. Sample usage:
//
//  THREAD_DBG << "Unlocking " << this << ENDL
//
// Note that the two macros must be used in a pair to ensure that the
// output is unlocked.
//
// TODO(alasdair): This is messy, and we need a proper thread-safe
// logging facility.  Keeping this for now as it's useful, but we
// should clean this up.
//
#ifdef DEBUG
#define THREAD_DBG MutexInternal::lock_stderr(); std::cerr
#define ENDL std::endl; MutexInternal::unlock_stderr();
#else
#define THREAD_DBG while (0) { std::cerr
#define ENDL std::endl; }
#endif

namespace MutexInternal {
class CompoundConditionVariable;
extern void lock_stderr();
extern void unlock_stderr();
}

class ThreadLatch;

// Allows a test to wait until one of its threads reaches a blocked
// state. When writing tests in which several threads contend for a
// lock, this can be used to ensure that the test threads are in a
// known state.
//
// Sample code:
//
// Class under test:
//
//   class WorkQueue {
//    public:
//     // Blocks waiting until a task is available
//     Task get_task();
//
//     void add_task(const Task& task);
//   };
//
// Test code
//  
//   void ProcessTask(WorkQueue *queue) {
//     Task task = queue->get_task();
//     task.process();
//   }
//
//   TEST_F(WorkQueueTest, GetTask) {
//     TestTask task;
//     WorkQueue queue;
//     thread thread(std::bind(ProcessTask, &queue));
//   
//     //  Wait until the thread has blocked. We expect it to be in the
//     //  ProcessTask method, waiting for get_task() to return.
//   	 ThreadMonitor::GetInstance()->WaitUntilBlocked(thread.get_id());
//     EXPECT_FALSE(task.is_processed());
//   
//     //  Add the task. This should ublock the thread.
//     queue.add_task(task);
//     thread.join();
//     EXPECT_TRUE(task.is_processed());
//   }
//

class ThreadMonitor {
 public:
  // Obtains an instance of ThreadMonitor. This should not be deleted.
  static ThreadMonitor* GetInstance();

  // Blocks the caller until the thread identified by "id" is blocked waiting
  // on a mutex or condition variable
  void WaitUntilBlocked(thread::id id);

  // Returns true if the given thread is blocked.
  bool IsBlocked(thread::id id);

 protected:
  friend class MutexInternal::_posix_mutex;
  friend class ::condition_variable;
  void OnThreadBlocked(thread::id id);
  void OnThreadReleased(thread::id id);

 private:
  ThreadMonitor();
  ~ThreadMonitor();
  ThreadLatch* GetThreadLatch(thread::id id);

  MutexInternal::CompoundConditionVariable* map_lock_;
  typedef std::map<thread::id, ThreadLatch*> ThreadMap;

  ThreadMap blocked_waiters_;

  static ThreadMonitor gMonitor;
};

#endif
