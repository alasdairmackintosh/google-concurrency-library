// Copyright 2009 Google Inc. All Rights Reserved.
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

// This file is a rudimentary test of the lock classes.

#include <string>

#include "mutex.h"
#include "condition_variable.h"
#include "thread.h"

#include "gmock/gmock.h"

using ::testing::InSequence;

class CheckPoint {
public:
  MOCK_METHOD1(Check, void(const std::string& id));
};

class MockMutex {
public:
  MOCK_METHOD0(lock, void());
  MOCK_METHOD0(try_lock, bool());
  MOCK_METHOD0(unlock, void());
};

TEST(LockGuardTest, Simple) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  lock_guard<MockMutex> l(mu);
  cp.Check("locked");
}

TEST(LockGuardTest, Adopt) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  mu.lock();
  lock_guard<MockMutex> l(mu, adopt_lock);
  cp.Check("locked");
}


TEST(UniqueLockTest, DefaultConstructor) {
  unique_lock<MockMutex> l;
  EXPECT_EQ((MockMutex*)NULL, l.mutex());
  EXPECT_FALSE(l.owns_lock());
}

TEST(UniqueLockTest, Simple) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  unique_lock<MockMutex> l(mu);
  EXPECT_EQ(&mu, l.mutex());
  cp.Check("locked");
}

TEST(UniqueLockTest, Unlock) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
    EXPECT_CALL(cp, Check("unlocked"));
  }

  unique_lock<MockMutex> l(mu);
  EXPECT_EQ(&mu, l.mutex());
  cp.Check("locked");
  l.unlock();
  EXPECT_EQ(&mu, l.mutex());
  cp.Check("unlocked");
  // Shouldn't unlock the lock a second time.
}

TEST(UniqueLockTest, Adopt) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  mu.lock();
  unique_lock<MockMutex> l(mu, adopt_lock);
  EXPECT_EQ(&mu, l.mutex());
  cp.Check("locked");
}

TEST(UniqueLockTest, Defer) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(cp, Check("unlocked"));
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  unique_lock<MockMutex> l(mu, defer_lock);
  cp.Check("unlocked");
  EXPECT_EQ(&mu, l.mutex());
  l.lock();
  cp.Check("locked");
  EXPECT_TRUE(l.owns_lock());
}

TEST(UniqueLockTest, ReleaseUnlocked) {
  MockMutex mu;
  unique_lock<MockMutex> l(mu, defer_lock);
  EXPECT_EQ(&mu, l.mutex());
  EXPECT_EQ(&mu, l.release());
  EXPECT_EQ((MockMutex*)NULL, l.mutex());
  EXPECT_EQ((MockMutex*)NULL, l.release());
}

TEST(UniqueLockTest, ReleaseLocked) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
    EXPECT_CALL(cp, Check("unlocked"));
  }

  unique_lock<MockMutex> l(mu);
  EXPECT_EQ(&mu, l.mutex());
  cp.Check("locked");
  EXPECT_EQ(&mu, l.release());
  cp.Check("unlocked");
  EXPECT_EQ((MockMutex*)NULL, l.mutex());
  EXPECT_EQ((MockMutex*)NULL, l.release());
}


struct BoolTrue {
  bool& b_;
  BoolTrue(bool& b) : b_(b) {}
  bool operator()() { return b_; }
};

struct ConditionVariableTest_Waits_Thread {
  mutex& mu_;
  condition_variable& condvar_;
  bool& ready_;
  ConditionVariableTest_Waits_Thread(
    mutex& mu, condition_variable& condvar, bool& ready)
    : mu_(mu), condvar_(condvar), ready_(ready) {}
  void operator()() {
    unique_lock<mutex> l(mu_);
    condvar_.wait(l, BoolTrue(ready_));
    EXPECT_TRUE(ready_);
  }
};

TEST(ConditionVariableTest, Waits) {
  mutex mu;
  condition_variable condvar;
  bool ready = false;
  thread waiter(ConditionVariableTest_Waits_Thread(mu, condvar, ready));
  {
    unique_lock<mutex> l(mu);
    this_thread::sleep_for(chrono::milliseconds(20));
  }
  condvar.notify_one();
  // In case the first wakeup causes the background thread to
  // continue, give it some time to fail.
  this_thread::sleep_for(chrono::milliseconds(20));
  {
    unique_lock<mutex> l(mu);
    ready = true;
  }
  // Notify doesn't have to be called under the lock.
  condvar.notify_one();
  // Would deadlock here if notify didn't work...
  waiter.join();
}

TEST(RecursiveLockTest, Simple) {
  recursive_mutex r_mu;
  r_mu.lock();
  r_mu.lock();
  r_mu.unlock();
  r_mu.unlock();
}
