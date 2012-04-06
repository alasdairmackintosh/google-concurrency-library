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
#include <iterator>
#include <string>
#include <vector>

#include "atomic.h"
#include "thread.h"

#include "gmock/gmock.h"

#include "barrier.h"

using testing::_;
using testing::Invoke;
using testing::InSequence;

using std::atomic;
using gcl::barrier;

static size_t kNumThreads = 3;
static size_t kZero = 0;

class BarrierTest : public testing::Test {
};

// Verifies that we cannot create a barrier with an illegal number of
// threads.
TEST_F(BarrierTest, InvalidConstructorArg) {
  try {
    barrier b(kZero);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
  try {
    barrier b(kZero, NULL);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Invokes await() on a barrier, and counts the number of exceptions
// thrown. If progress_count is non-null, it is incremented before
// calling await(), and again afterwards. The thread in which await()
// returnsd true will increment progress_count one additional time.
static void WaitForBarrierCountExceptions(barrier* b,
                                          atomic<int> *progress_count,
                                          atomic<int> *exception_count) {
  try {
    if (progress_count != NULL) {
      (*progress_count)++;
    }
    bool result = b->await();
    if (progress_count != NULL) {
      (*progress_count)++;
      if (result) {
        (*progress_count)++;
      }
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

// Verify that if we try to call await with too many threads, we get
// an exception
TEST_F(BarrierTest, TooManyThreads) {
  barrier b(kNumThreads - 1);
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
  EXPECT_EQ(1, num_exceptions.load());
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

  // We expect one of the threads to have incremented its progress
  // count to 3, because await() returned true. All other threads will
  // have incremented to 2.
  size_t num_at_2 = 0;
  int num_at_3 = 0;
  for (size_t i = 0; i < kNumThreads; i++) {
    int progress = counters[i].load();
    if (progress == 2) {
      num_at_2++;
    } else if (progress == 3) {
      num_at_3++;
    }
  }
  EXPECT_EQ(1, num_at_3);
  EXPECT_EQ(kNumThreads - 1, num_at_2);

  for (size_t i = 0; i < kNumThreads; i++) {
    delete threads[i];
  }
}
