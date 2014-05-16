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

// This file is a test of the notifying_barrier class

#include <algorithm>
#include <string>

#include "atomic.h"
#include "thread.h"
#include "scoped_guard.h"

#include "gmock/gmock.h"

#include "notifying_barrier.h"

using testing::_;

using std::atomic;
using gcl::notifying_barrier;
using gcl::scoped_guard;

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::function;
#else
using std::tr1::function;
#endif

static int kNumThreads = 3;
static int kZero = 0;

class NotifyingBarrierTest : public testing::Test {
};

int num_threads_completion() {
  return kNumThreads;
}

TEST_F(NotifyingBarrierTest, Constructors) {
  notifying_barrier b2(kNumThreads, &num_threads_completion);
}

// Verifies that we cannot create a notifying_barrier with an illegal number of
// threads.
TEST_F(NotifyingBarrierTest, InvalidConstructorArg) {
  try {
    notifying_barrier b(kZero, &num_threads_completion);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
  try {
    function<int()> completion_fn = NULL;
    notifying_barrier b(kZero, completion_fn);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Invokes arrive_and_wait() on a notifying_barrier, and counts the number of
// exceptions thrown. If progress_count is non-null, it is incremented before
// calling arrive_and_wait(), and again afterwards.
static void WaitForNotifyingBarrierCountExceptions(notifying_barrier* b,
                                          atomic<int> *progress_count,
                                          atomic<int> *exception_count) {
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

// Invoked by a notifying_barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForNotifyingBarrierCountExceptions)
static void WaitFn(atomic<int>* counters) {
  for (int i = 0; i < kNumThreads; i++) {
    EXPECT_EQ(1, counters[i].load());
  }
}


TEST_F(NotifyingBarrierTest, CorrectNumberOfThreads) {
  notifying_barrier b(kNumThreads, &num_threads_completion);
  atomic<int> num_exceptions;
  num_exceptions = 0;

  thread* threads[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForNotifyingBarrierCountExceptions,
                                      &b, static_cast<atomic<int>*>(NULL),
                                      &num_exceptions));
  }
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (int i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(NotifyingBarrierTest, FunctionInvocation) {
  notifying_barrier b(kNumThreads, &num_threads_completion);
  atomic<int> num_exceptions;
  num_exceptions = 0;
  atomic<int> counters[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  std::function<void()> wait_fn = std::bind(WaitFn, counters + 0);

  thread* threads[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForNotifyingBarrierCountExceptions,
                                      &b, counters + i, &num_exceptions));
  }
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (int i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    EXPECT_EQ(2, progress);
  }

  for (int i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Invoked by a notifying_barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForNotifyingBarrierCountExceptions)
static int WaitFnAndReset(int* num_calls,
                             atomic<int>* counters) {
  (*num_calls)++;
  std::cerr << "num_calls = " << *num_calls << "\n";

  // only the first notifying_barrier is guaranteed to restrict all
  // kNumThreads threads
  if (*num_calls == 1) {
    for (int i = 0; i < kNumThreads; i++) {
      EXPECT_EQ(*num_calls, counters[i].load());
    }
  }
  else {
    EXPECT_EQ(*num_calls, counters[0].load());
  }
  return 1;
}

static void WaitForNotifyingBarrierCountExceptionsRetry(bool try_again,
                                               notifying_barrier* b,
                                               atomic<int> *progress_count,
                                               atomic<int> *exception_count) {
  try {
    (*progress_count)++;
    b->arrive_and_wait();
    (*progress_count)++;
    if (try_again) {
      b->arrive_and_wait();
      // progress count of other threads may not have incremented since first
      // notifying_barrier, since only one thread needs to pass through this one.
      (*progress_count)++;
    }
  } catch (std::logic_error e) {
    (*exception_count)++;
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(NotifyingBarrierTest, FunctionInvocationAndReset) {
  atomic<int> num_exceptions;
  num_exceptions = 0;
  atomic<int> counters[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  int num_calls = 0;
  std::function<int()> wait_fn =
      std::bind(WaitFnAndReset, &num_calls, counters + 0);
  // this notifying_barrier first holds back kNumThreads, then only one
  // on subsequent tries
  notifying_barrier b(kNumThreads, wait_fn);

  thread* threads[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForNotifyingBarrierCountExceptionsRetry,
                                      i == 0,
                                      &b,
                                      counters + i,
                                      &num_exceptions));
  }
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (int i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    int expected_value = i == 0 ? 3 : 2;
    EXPECT_EQ(expected_value, progress);
  }

  for (int i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

#ifdef HAS_CXX11_RVREF
static int WaitAndCountFn(int* num_calls) {
  (*num_calls)++;
  return kNumThreads;
}

void CountDownAndWaitWithGuard(notifying_barrier& notifying_barrier) {
  scoped_guard g = notifying_barrier.arrive_and_wait_guard();
}

TEST_F(NotifyingBarrierTest, ScopedGuardCountDown) {
  int num_calls = 0;
  std::function<int()> wait_fn = std::bind(WaitAndCountFn, &num_calls);
  notifying_barrier b(2, wait_fn);
  thread t1(std::bind(CountDownAndWaitWithGuard, std::ref(b)));
  thread t2(std::bind(CountDownAndWaitWithGuard, std::ref(b)));
  t1.join();
  t2.join();
  // Both threads should have counted down, so the completion function should
  // have been invoked.
  ASSERT_EQ(1, num_calls);
}
#endif
