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

#ifndef STD_TESTING_THREADBLOCKER_
#define STD_TESTING_THREADBLOCKER_

#include <iostream>

#include "mutex.h"
#include "thread.h"

#include "countdown_latch.h"
#include "test_mutex.h"

using gcl::countdown_latch;

// A ThreadBlocker will block when it is invoked from a given thread,
// but will not block when invoked from other threads. It enables the
// creation of test cases where two similar actions are performed in
// two separate threads. If the threads are A and B then a typical
// sequence might be:
//
// Start an operation in thread A. Waiting until thread A is blocked
// by the ThreadBlocker.
//
// Perform the same operation in thread B. The operation will not block.
//
// Resume thread A when thread B has finished. Verify the results.
//
// A ThreadBlocker is used in conjunciton with a BlockableThread.
// Sample useage would be:
//
//  void Action(ThreadBlocker& blocker) {
//    ...
//    // Will only block when called from blockable thread created below.
//    blocker.Block();
//  }
//
//  void Test() {
//    ThreadBlocker blocker();
//    BlockableThread thr(std::bind(Action, blocker), &blocker);
//    blocker.Start();
//
//    // Wait until 'thr' is blocked.
//    ThreadMonitor::GetInstance()->WaitUntilBlocked(thr.get_id());
//    // Invoke Action() in another thread. Will not block.
//    ...
//    blocker.Unlock();
//    thr.join();
//
class ThreadBlocker {
 public:
  // Creates a ThreadBlocker that will block on the first call to
  // Block()
  ThreadBlocker();

  // Creates a ThreadBlocker that will block on the count'th call to
  // Block()
  explicit ThreadBlocker(int count);

  // Starts the BlockableThread associated with this ThreadBlocker
  void Start();

  // Blocks the caller iff it is invoked from the associated
  // BlockableThread, and the count has been reached. Note that
  // subsequent calls to this method (after count has been reached)
  // will not block.
  void Block();

  // Unlocks the blocked thread. If called before the thread has
  // blocked, the unlock operation is a no-op, and the thread will
  // still subsequently block when the conditions are met.
  void Unlock();

private:
  friend class BlockableThread;
  void SetThread(thread::id id);
  void WaitToStart();

  thread::id id_;
  mutex count_mutex_;
  int count_;
  countdown_latch block_latch_;
  countdown_latch start_latch_;
};

// A thread used in conjunction with a ThreadBlocker
class BlockableThread : public thread {
public:
  // Creates a new BlockableThread associated with the given
  // ThreadBlocker. A ThreadBlocker should only be associated with one thread.
  template<class F> explicit BlockableThread(F f, ThreadBlocker* blocker);
private:
  void init(ThreadBlocker* blocker) {
    blocker->SetThread(this_thread::get_id());
    blocker->WaitToStart();
    f_();
  }
  std::function<void()> f_;
  
  // Disallow copy and assign
  BlockableThread(const BlockableThread&);
  void operator=(const BlockableThread&);
};

template<class F>
BlockableThread::BlockableThread(F f, ThreadBlocker* blocker)
: thread(std::bind(&BlockableThread::init, this, blocker)),
  f_(f) {
}

#endif  // STD_TESTING_THREADBLOCKER_
