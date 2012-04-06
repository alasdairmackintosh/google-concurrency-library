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
#include "barrier.h"

#include "debug.h"

#include "atomic.h"
#include "thread.h"

#include "gmock/gmock.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#if defined(__GXX_EXPERIMENTAL_CXX0X__)
using std::bind;
using std::function;
#else
using std::tr1::bind;
using std::tr1::function;
#endif

using testing::_;
using testing::Invoke;
using testing::InSequence;

using std::atomic_int;
using gcl::barrier;

static size_t kNumThreads = 3;


// Invokes count_down_and_wait() on a barrier, and counts the number of
// exceptions thrown. If progress_count is non-null, it is incremented before
// calling count_down_and_wait(), and again afterwards.
static void WaitForBarrierCountExceptions(barrier* b,
                                          atomic_int *progress_count,
                                          atomic_int *exception_count) {
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

// Invoked by a barrier after all threads have called count_down_and_wait(), but
// before any are released. The value of each counter should be 1.
// (See the handling of progress_count in WaitForBarrierCountExceptions)
static void WaitFn(atomic_int* counters) {
  for (size_t i = 0; i < kNumThreads; i++) {
    EXPECT_EQ(1, counters[i].load());
  }
}

class BarrierTest : public testing::Test {
 protected:
  void RunBarrierThreads(barrier* b, size_t n_threads) {
    atomic_int num_exceptions;
    num_exceptions = 0;

    thread* threads[n_threads];
    for (size_t i = 0; i < n_threads; i++) {
      threads[i] = new thread(bind(WaitForBarrierCountExceptions,
                                   b, static_cast<atomic_int*>(NULL),
                                   &num_exceptions));
    }
    for (size_t i = 0; i < n_threads; i++) {
      threads[i]->join();
    }
    EXPECT_EQ(0, num_exceptions.load());
  }
};

TEST_F(BarrierTest, CorrectNumberOfThreads) {
  barrier b(kNumThreads);
  atomic_int num_exceptions;
  num_exceptions = 0;

  thread* threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(bind(WaitForBarrierCountExceptions,
                                 &b, static_cast<atomic_int*>(NULL),
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
  atomic_int num_exceptions;
  num_exceptions = 0;
  atomic_int counters[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    counters[i] = 0;
  }
  function<void()> wait_fn = bind(WaitFn, counters);

  thread* threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; i++) {
    threads[i] = new thread(bind(WaitForBarrierCountExceptions,
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

static void ResetBarrier(barrier* b, size_t* n_threads) {
  --(*n_threads);
  b->reset(*n_threads);
}

TEST_F(BarrierTest, Reset) {
  size_t n_threads = kNumThreads;
  barrier b(n_threads);
  b.reset(bind(ResetBarrier, &b, &n_threads));
  RunBarrierThreads(&b, n_threads);
  EXPECT_EQ(n_threads, kNumThreads - 1);
  RunBarrierThreads(&b, n_threads);
  EXPECT_EQ(n_threads, kNumThreads - 2);
  RunBarrierThreads(&b, n_threads);
  EXPECT_EQ(n_threads, kNumThreads - 3);
}
