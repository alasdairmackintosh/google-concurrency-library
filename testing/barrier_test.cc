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

// This file is a test of the barrier class

#include <algorithm>
#include <string>

#include "atomic.h"
#include "thread.h"

#include "gmock/gmock.h"

#include "barrier.h"

using testing::_;

using std::atomic;
using gcl::barrier;

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::function;
#else
using std::tr1::function;
#endif

static size_t kNumThreads = 3;
static size_t kZero = 0;

class BarrierTest : public testing::Test {
};

void void_completion() {
}

size_t size_completion() {
  return 0;
}

TEST_F(BarrierTest, Constructors) {
  barrier b1(kNumThreads, &void_completion);
  barrier b2(kNumThreads, &size_completion);
}

// Verifies that we cannot create a barrier with an illegal number of
// threads.
TEST_F(BarrierTest, InvalidConstructorArg) {
  try {
    barrier b(kZero);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
  try {
    function<void()> completion_fn = NULL;
    barrier b(kZero, completion_fn);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Invokes count_down_and_wait() on a barrier, and counts the number of
// exceptions thrown. If progress_count is non-null, it is incremented before
// calling count_down_and_wait(), and again afterwards.
static void WaitForBarrierCountExceptions(barrier* b,
                                          atomic<int> *progress_count,
                                          atomic<int> *exception_count) {
  try {
    if (progress_count != NULL) {
      (*progress_count)++;
    }
    b->count_down_and_wait();
    if (progress_count != NULL) {
      (*progress_count)++;
    }
  } catch (std::logic_error e) {
    (*exception_count)++;
  }
}

// Invoked by a barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForBarrierCountExceptions)
static void WaitFn(atomic<int>* counters) {
  for (size_t i = 0; i < kNumThreads; i++) {
    EXPECT_EQ(1, counters[i].load());
  }
}


TEST_F(BarrierTest, CorrectNumberOfThreads) {
  barrier b(kNumThreads);
  atomic<int> num_exceptions;
  num_exceptions = 0;

  thread* threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForBarrierCountExceptions,
                                      &b, static_cast<atomic<int>*>(NULL),
                                      &num_exceptions));
  }
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (size_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(BarrierTest, FunctionInvocation) {
  barrier b(kNumThreads);
  atomic<int> num_exceptions;
  num_exceptions = 0;
  atomic<int> counters[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  std::function<void()> wait_fn = std::bind(WaitFn, counters + 0);

  thread* threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForBarrierCountExceptions,
                                      &b, counters + i, &num_exceptions));
  }
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (size_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    EXPECT_EQ(2, progress);
  }

  for (size_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}

// Invoked by a barrier after all threads have called await(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForBarrierCountExceptions)
static size_t WaitFnAndReset(int* num_calls,
                             atomic<int>* counters) {
  (*num_calls)++;
  std::cerr << "num_calls = " << *num_calls << "\n";

  // only the first barrier is guaranteed to restrict all
  // kNumThreads threads
  if (*num_calls == 1) {
    for (size_t i = 0; i < kNumThreads; i++) {
      EXPECT_EQ(*num_calls, counters[i].load());
    }
  }
  else {
    EXPECT_EQ(*num_calls, counters[0].load());
  }
  return 1;
}

static void WaitForBarrierCountExceptionsRetry(bool try_again,
                                               barrier* b,
                                               atomic<int> *progress_count,
                                               atomic<int> *exception_count) {
  try {
    (*progress_count)++;
    b->count_down_and_wait();
    (*progress_count)++;
    if (try_again) {
      b->count_down_and_wait();
      // progress count of other threads may not have incremented since first
      // barrier, since only one thread needs to pass through this one.
      (*progress_count)++;
    }
  } catch (std::logic_error e) {
    (*exception_count)++;
  }
}

// Verify that a registered function is correctly invoked after all
// threads have waited.
TEST_F(BarrierTest, FunctionInvocationAndReset) {
  atomic<int> num_exceptions;
  num_exceptions = 0;
  atomic<int> counters[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  int num_calls = 0;
  std::function<size_t()> wait_fn =
      std::bind(WaitFnAndReset, &num_calls, counters + 0);
  // this barrier first holds back kNumThreads, then only one
  // on subsequent tries
  barrier b(kNumThreads, wait_fn);

  thread* threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(std::bind(WaitForBarrierCountExceptionsRetry,
                                      i == 0,
                                      &b,
                                      counters + i,
                                      &num_exceptions));
  }
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i]->join();
  }
  EXPECT_EQ(0, num_exceptions.load());

  for (size_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    int expected_value = i == 0 ? 3 : 2;
    EXPECT_EQ(expected_value, progress);
  }

  for (size_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}
