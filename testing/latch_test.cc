// Copyright 2013 Google Inc. All Rights Reserved.
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

// Unit tests for latch

#include "functional.h"

#include "atomic.h"
#include "thread.h"
#include "scoped_guard.h"

#include "latch.h"

#include "gmock/gmock.h"

using namespace std;
using gcl::latch;
using gcl::scoped_guard;
using testing::_;

class LatchTest : public testing::Test {
};

void WaitForLatch(latch& latch) {
  std::cerr << "WaitForLatch " << this_thread::get_id() << "\n";
  latch.wait();
  std::cerr << "WaitForLatch waited " << this_thread::get_id() << "\n";
}

void TryWaitForLatch(latch& test_latch, latch& started_latch, atomic_bool* finished) {
  started_latch.count_down(1);
  while (!test_latch.try_wait()) {
    // spin;
  }
  *finished = true;
}

void WaitForLatchAndDecrement(latch& to_wait,
                              latch& decrement) {
  to_wait.wait();
  decrement.count_down(1);
  EXPECT_TRUE(to_wait.try_wait());
  EXPECT_TRUE(to_wait.try_wait());
}

void DecrementAndWaitForLatch(latch& decrement,
                              latch& to_wait) {
  decrement.count_down(1);
  to_wait.wait();
  EXPECT_TRUE(to_wait.try_wait());
  EXPECT_TRUE(decrement.try_wait());
}

// Tests two threads waiting on a single latch
TEST_F(LatchTest, TwoThreads) {
  latch latch(2);
  thread t1(std::bind(WaitForLatch, std::ref(latch)));
  thread t2(std::bind(WaitForLatch, std::ref(latch)));
  std::cerr << "Counting down " << this_thread::get_id() << "\n";
  latch.count_down(1);
  std::cerr << "Counting down " << this_thread::get_id() << "\n";
  latch.count_down(1);
  t1.join();
  t2.join();
}

// Tests two threads try_waiting on a single latch
TEST_F(LatchTest, TwoThreadsTryWait) {
  latch test_latch(1);
  latch started_latch(2);
  atomic_bool finished1;
  finished1 = false;
  atomic_bool finished2;
  finished2 = false;

  thread t1(std::bind(TryWaitForLatch,
                      std::ref(test_latch),
                      std::ref(started_latch),
                      &finished1));
  thread t2(std::bind(TryWaitForLatch,
                      std::ref(test_latch),
                      std::ref(started_latch),
                      &finished2));
  started_latch.wait();
  ASSERT_FALSE(finished1);
  ASSERT_FALSE(finished2);
  test_latch.count_down(1);
  t1.join();
  t2.join();
  ASSERT_TRUE(finished1);
  ASSERT_TRUE(finished2);
}

// Tests two threads waiting on a latch that has already
// been decremented.
TEST_F(LatchTest, TwoThreadsPreDecremented) {
  latch latch(2);
  latch.count_down(1);
  latch.count_down(1);
  thread t1(std::bind(WaitForLatch, std::ref(latch)));
  thread t2(std::bind(WaitForLatch, std::ref(latch)));
  t1.join();
  t2.join();
}

// Tests two threads waiting and decrementing two latches
TEST_F(LatchTest, TwoThreadsTwoLatches) {
  latch first(1);
  latch second(1);
  thread t1(std::bind(
      WaitForLatchAndDecrement, std::ref(first), std::ref(second)));
  thread t2(std::bind(
      DecrementAndWaitForLatch, std::ref(first), std::ref(second)));
  t1.join();
  t2.join();
}

#ifdef HAS_CXX11_RVREF
void ArriveWithGuard(latch& latch) {
  scoped_guard g = latch.arrive_guard();
}

void WaitWithGuard(latch& latch) {
  scoped_guard g = latch.wait_guard();
}

void ArriveAndWaitWithGuard(latch& latch) {
  scoped_guard g = latch.arrive_and_wait_guard();
}

TEST_F(LatchTest, ScopedGuardArrive) {
  latch latch(2);
  thread t1(std::bind(
      ArriveWithGuard, std::ref(latch)));
  thread t2(std::bind(
      ArriveWithGuard, std::ref(latch)));
  t1.join();
  t2.join();
  // Both threads should have counted down
  ASSERT_TRUE(latch.try_wait());
}

TEST_F(LatchTest, ScopedGuardWait) {
  latch latch(1);
  thread t1(std::bind(
      ArriveAndWaitWithGuard, std::ref(latch)));
  thread t2(std::bind(
      WaitWithGuard, std::ref(latch)));
  t1.join();
  t2.join();
  // Both threads should have completed. and one should have counted down.
  ASSERT_TRUE(latch.try_wait());
}
#endif
