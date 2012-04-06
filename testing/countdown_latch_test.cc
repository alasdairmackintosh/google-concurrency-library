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

// Unit tests for countdown_latch

#include "functional.h"

#include "thread.h"

#include "countdown_latch.h"
#include "test_mutex.h"

#include "gmock/gmock.h"

using namespace std;
using testing::_;

class CountdownLatchTest : public testing::Test {
};

void WaitForLatch(countdown_latch& latch) {
  std::cerr << "WaitForLatch " << this_thread::get_id() << "\n";
  latch.wait();
  std::cerr << "WaitForLatch waited " << this_thread::get_id() << "\n";
  EXPECT_EQ(latch.get_count(), 0);
}

void WaitForLatchAndDecrement(countdown_latch& to_wait,
                              countdown_latch& decrement) {
  to_wait.wait();
  decrement.count_down();
  EXPECT_EQ(to_wait.get_count(), 0);
  EXPECT_EQ(decrement.get_count(), 0);
}

void DecrementAndWaitForLatch(countdown_latch& decrement,
                              countdown_latch& to_wait) {
  decrement.count_down();
  to_wait.wait();
  EXPECT_EQ(to_wait.get_count(), 0);
  EXPECT_EQ(decrement.get_count(), 0);
}

// Tests two threads waiting on a single countdown_latch
TEST_F(CountdownLatchTest, TwoThreads) {
  countdown_latch latch(2);
  thread t1(std::bind(WaitForLatch, std::ref(latch)));
  thread t2(std::bind(WaitForLatch, std::ref(latch)));
  std::cerr << "Counting down " << this_thread::get_id() << "\n";
  latch.count_down();
  std::cerr << "Counting down " << this_thread::get_id() << "\n";
  latch.count_down();
  t1.join();
  t2.join();
}

// Tests two threads waiting on a countdown_latch that has already
// been decremented.
TEST_F(CountdownLatchTest, TwoThreadsPreDecremented) {
  countdown_latch latch(2);
  latch.count_down();
  latch.count_down();
  thread t1(std::bind(WaitForLatch, std::ref(latch)));
  thread t2(std::bind(WaitForLatch, std::ref(latch)));
  t1.join();
  t2.join();
}

// Tests two threads waiting and decrementing two latches
TEST_F(CountdownLatchTest, TwoThreadsTwoLatches) {
  countdown_latch first(1);
  countdown_latch second(1);
  thread t1(std::bind(
      WaitForLatchAndDecrement, std::ref(first), std::ref(second)));
  thread t2(std::bind(
      DecrementAndWaitForLatch, std::ref(first), std::ref(second)));
  t1.join();
  t2.join();
}
