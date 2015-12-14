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

// This file is a test of the flex_barrier class

#include <algorithm>
#include <string>

#include <atomic>
#include <thread>
#include "scoped_guard.h"

#include "gmock/gmock.h"

#include "flex_barrier.h"

using testing::_;

using gcl::flex_barrier;
using gcl::scoped_guard;

static std::ptrdiff_t kNumThreads = 3;
static std::ptrdiff_t kZero = 0;

class NotifyingBarrierTest : public testing::Test {
};

std::ptrdiff_t num_threads_completion() {
  return kNumThreads;
}

TEST_F(NotifyingBarrierTest, Constructors) {
  flex_barrier b2(kNumThreads, &num_threads_completion);
}

// Verifies that we cannot create a flex_barrier with an illegal number of
// threads.
TEST_F(NotifyingBarrierTest, InvalidConstructorArg) {
  try {
    flex_barrier b(kZero, &num_threads_completion);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
  try {
    std::function<std::ptrdiff_t()> completion_fn = NULL;
    flex_barrier b(kZero, completion_fn);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Invokes arrive_and_wait() on a flex_barrier, and counts the number of
// exceptions thrown. If progress_count is non-null, it is incremented before
// calling arrive_and_wait(), and again afterwards.
static void WaitForNotifyingBarrierCountExceptions(
    flex_barrier* b, std::atomic<int>* progress_count,
    std::atomic<int>* exception_count) {
  try {
    if (progress_count != NULL) {
      (*progress_count)++;
    }
    b->arrive_and_wait();
    if (progress_count != NULL) {
      (*progress_count)++;
    }
  } catch (std::logic_error e) {
    (*exception_count)++;
  }
}

// Invoked by a flex_barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForNotifyingBarrierCountExceptions)
static void WaitFn(std::atomic<int>* counters) {
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    EXPECT_EQ(1, counters[i].load());
  }
}


TEST_F(NotifyingBarrierTest, CorrectNumberOfThreads) {
  flex_barrier b(kNumThreads, &num_threads_completion);
  std::atomic<int> num_exceptions;
  num_exceptions = 0;

  std::thread* threads[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i] = new std::thread(
        std::bind(WaitForNotifyingBarrierCountExceptions, &b,
                  static_cast<std::atomic<int>*>(NULL), &num_exceptions));
  }
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(NotifyingBarrierTest, FunctionInvocation) {
  flex_barrier b(kNumThreads, &num_threads_completion);
  std::atomic<int> num_exceptions;
  num_exceptions = 0;
  std::atomic<int> counters[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  std::function<void()> wait_fn = std::bind(WaitFn, counters + 0);

  std::thread* threads[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i] =
        new std::thread(std::bind(WaitForNotifyingBarrierCountExceptions, &b,
                                  counters + i, &num_exceptions));
  }
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    EXPECT_EQ(2, progress);
  }

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Invoked by a flex_barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForNotifyingBarrierCountExceptions)
static std::ptrdiff_t WaitFnAndReset(int* num_calls,
                                     std::atomic<int>* counters) {
  (*num_calls)++;
  // only the first flex_barrier is guaranteed to restrict all
  // kNumThreads threads
  if (*num_calls == 1) {
    for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
      EXPECT_EQ(*num_calls, counters[i].load());
    }
  }
  else {
    EXPECT_EQ(*num_calls, counters[0].load());
  }
  return 1;
}

static void WaitForNotifyingBarrierCountExceptionsRetry(
    bool try_again, flex_barrier* b, std::atomic<int>* progress_count,
    std::atomic<int>* exception_count) {
  try {
    (*progress_count)++;
    b->arrive_and_wait();
    (*progress_count)++;
    if (try_again) {
      b->arrive_and_wait();
      // progress count of other threads may not have incremented since first
      // flex_barrier, since only one thread needs to pass through this one.
      (*progress_count)++;
    }
  } catch (std::logic_error e) {
    (*exception_count)++;
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(NotifyingBarrierTest, FunctionInvocationAndReset) {
  std::atomic<int> num_exceptions;
  num_exceptions = 0;
  std::atomic<int> counters[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  int num_calls = 0;
  std::function<std::ptrdiff_t()> wait_fn =
      std::bind(WaitFnAndReset, &num_calls, counters + 0);
  // this flex_barrier first holds back kNumThreads, then only one
  // on subsequent tries
  flex_barrier b(kNumThreads, wait_fn);

  std::thread* threads[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i] =
        new std::thread(std::bind(WaitForNotifyingBarrierCountExceptionsRetry,
                                  i == 0, &b, counters + i, &num_exceptions));
  }
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    int expected_value = i == 0 ? 3 : 2;
    EXPECT_EQ(expected_value, progress);
  }

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

static void ArriveAndDrop(flex_barrier* b) {
  b->arrive_and_drop();
}

TEST_F(NotifyingBarrierTest, ArriveAndDrop) {
  flex_barrier b(kNumThreads + 1, &num_threads_completion);
  std::atomic<int> num_exceptions;
  num_exceptions = 0;

  std::thread* threads[kNumThreads + 1];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i] = new std::thread(
        std::bind(WaitForNotifyingBarrierCountExceptions, &b,
                  static_cast<std::atomic<int>*>(NULL), &num_exceptions));
  }
  threads[kNumThreads] = new std::thread(std::bind(ArriveAndDrop, &b));
  for (std::ptrdiff_t i = 0; i < kNumThreads + 1; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (std::ptrdiff_t i = 0; i < kNumThreads + 1; i++) {
    delete threads[i];
  }
}

TEST_F(NotifyingBarrierTest, ArriveAndDropWithCompletion) {
  std::atomic<int> num_exceptions;
  num_exceptions = 0;
  std::atomic<int> counters[kNumThreads];
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  int num_calls = 0;
  std::function<std::ptrdiff_t()> wait_fn =
      std::bind(WaitFnAndReset, &num_calls, counters + 0);

  // this flex_barrier first holds back kNumThreads + 1, then only one
  // on subsequent tries
  flex_barrier b(kNumThreads + 1, wait_fn);

  std::thread* threads[kNumThreads + 1];
  threads[kNumThreads] = new std::thread(std::bind(ArriveAndDrop, &b));
  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    threads[i] =
        new std::thread(std::bind(WaitForNotifyingBarrierCountExceptionsRetry,
                                  i == 0, &b, counters + i, &num_exceptions));
  }

  for (std::ptrdiff_t i = 0; i < kNumThreads + 1; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (std::ptrdiff_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    int expected_value = i == 0 ? 3 : 2;
    EXPECT_EQ(expected_value, progress);
  }

  for (std::ptrdiff_t i = 0; i < kNumThreads + 1; i++) {
    delete threads[i];
  }
}
