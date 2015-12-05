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

// Tests thread behaviour using the test mutex classes. Verifies that
// the test framework allows us to block until a thread reaches a
// particular state.

#include "functional.h"

#include "mutex.h"
#include "thread.h"

#include "test_mutex.h"

#include "gmock/gmock.h"

static const int kNumThreads = 100;

// Sets 'id' to the id of this thread.
void SetThreadId(thread::id* id) {
  *id = this_thread::get_id();
}

static void LockMutex(mutex& mu, bool* ready) {
  mu.lock();
  *ready = true;
  mu.unlock();
}

// Verifies that wait_until_blocked will wait until a thread reaches
TEST(ThreadTest, WaitUntilBlocked) {
  mutex mu;
  mu.lock();
  bool ready = false;
  thread thr(std::bind(LockMutex, std::ref(mu), &ready));
  ThreadMonitor::GetInstance()->WaitUntilBlocked(thr.get_id());
  EXPECT_FALSE(ready);
  mu.unlock();
  thr.join();
  EXPECT_TRUE(ready);
}

TEST(ThreadTest, ManyThreads) {
  mutex mu;
  mu.lock();
  thread* threads[kNumThreads];
  bool ready[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    ready[i] = false;
    threads[i] = new thread(std::bind(LockMutex, std::ref(mu), ready + i));
  }
  mu.unlock();
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
    EXPECT_TRUE(ready[i]);
    delete threads[i];
  }
}
