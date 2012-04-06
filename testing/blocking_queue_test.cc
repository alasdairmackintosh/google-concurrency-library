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

// This file is a test of the blocking_queue class

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "atomic.h"

#include "blocking_queue.h"

#include "gmock/gmock.h"

using testing::_;
using testing::Invoke;
using testing::InSequence;

using std::atomic;
using std::memory_order_relaxed;
using gcl::blocking_queue;

size_t kSize = 3;
size_t kLargeSize = 300;
size_t kZero = 0;

class BlockingQueueTest : public testing::Test {
 protected:
  // Pushes a standard set of numbers onto the queue
  void push_elements(blocking_queue<int>& queue) {
    ASSERT_EQ(kZero, queue.size());
    for(int i = 0; i < static_cast<int>(kSize); i++) {
      ASSERT_TRUE(queue.try_push(i));
    }
    ASSERT_EQ(kSize, queue.size());
  }

  void pop_elements(blocking_queue<int>& queue) {
  // Pops a standard set of numbers from the queue, and validates the result.
    for(int i = 0; i < static_cast<int>(kSize); i++) {
      int popped;
      ASSERT_TRUE(queue.try_pop(popped));
      ASSERT_EQ(i, popped);
    }
    ASSERT_EQ(kZero, queue.size());
    ASSERT_TRUE(queue.empty());
  }
};

// Verifies that we cannot create a queue of size zero
TEST_F(BlockingQueueTest, InvalidArg) {
  try {
    blocking_queue<int> queue(kZero);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Verifies that we can create a queue from iterators.
TEST_F(BlockingQueueTest, CreateFromIterators) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  values.push_back(4);
  blocking_queue<int> queue(values.size() + 1, ++values.begin(), values.end());
  ASSERT_EQ(values.size() - 1, queue.size());
  size_t queue_size = queue.size();
  for(size_t i = 0; i < queue_size; i++) {
    int popped;
    ASSERT_TRUE(queue.try_pop(popped));
    ASSERT_EQ(values[i+1], popped);
  }
  ASSERT_EQ(kZero, queue.size());
  ASSERT_TRUE(queue.empty());
}

// Verifies that we cannot create a queue from iterators where the
// maximum size is less than that defined by the iterators
TEST_F(BlockingQueueTest, InvalidIterators) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  try {
    blocking_queue<int> queue(2, values.begin(), values.end());
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Verify basic push/pop operations.
TEST_F(BlockingQueueTest, Simple) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  pop_elements(queue);
}

// Verify assignment operator
TEST_F(BlockingQueueTest, AssignmentOperator) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  blocking_queue<int> new_queue;
  new_queue = queue;
  pop_elements(new_queue);
  ASSERT_EQ(kSize, queue.size());
}

// Verify copy constructor
TEST_F(BlockingQueueTest, CopyConstructor) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  blocking_queue<int> new_queue(queue);
  // Verify that altering the elements of new_queue does not affect
  // queue, i.e. new_queue is a gneuine copy
  pop_elements(new_queue);
  ASSERT_EQ(kZero, new_queue.size());
  ASSERT_EQ(kSize, queue.size());
}

// Verify that try_pop fails when the queue is empty, but succeeds when a new
// element is added.
TEST_F(BlockingQueueTest, TryPop) {
  blocking_queue<int> queue(kSize);
  int result;
  ASSERT_FALSE(queue.try_pop(result));
  ASSERT_TRUE(queue.try_push(1));
  ASSERT_TRUE(queue.try_pop(result));
  ASSERT_EQ(1, result);
  ASSERT_EQ(kZero, queue.size());
}

// Verify that try_push succeeds until we exceed the size imit
TEST_F(BlockingQueueTest, TryPush) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  ASSERT_FALSE(queue.try_push(42));

  // Pop an element off the front. We should now be able to push.
  ASSERT_EQ(0, queue.pop());
  ASSERT_TRUE(queue.try_push(42));

  ASSERT_EQ(1, queue.pop());
  ASSERT_EQ(2, queue.pop());
  ASSERT_EQ(42, queue.pop());
  ASSERT_EQ(kZero, queue.size());
}

// Verify that we cannot write to a closed queue
TEST_F(BlockingQueueTest, TryPushClosed) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  queue.close();
  try {
    queue.push(0);
    FAIL();
  } catch (gcl::closed_error expected) {
  }
}

// Verify that we cannot read from a closed queue once it has been
// emptied.
TEST_F(BlockingQueueTest, TryPopClosed) {
  blocking_queue<int> queue(kSize);
  push_elements(queue);
  queue.close();
  pop_elements(queue);
  ASSERT_TRUE(queue.is_closed());
  try {
    queue.pop();
    FAIL();
  } catch (gcl::closed_error expected) {
  }
}


// The following tests push elements onto the queue from one thread, and read
// from another. They will test the wait/notify mechanism (and they show no
// errors when run with the ThreadSanitizer) but they are not deterministic.
// TODO(alasdair): add tests using new thread blocking mechanism.

static void DoPush(blocking_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    queue->push(i);
  }
}

static void DoPushNegative(blocking_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    queue->push(-1 - i);
  }
}

static void DoPop(blocking_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    ASSERT_EQ(i, queue->pop());
  }
}

static void DoPopPositiveOrNegative(blocking_queue<int> *queue) {
  int last_positive = -1;
  int last_negative = 0;
  // One thread is pushing increasing positive numbers, starting from 0, while
  // the other is pushing decreasing negative numbers starting from -1. Verify
  // that the popped number is increasing or decreasing as appropriate. (The
  // numbers will be interleaved.)
  for (int i = 0; i < static_cast<int>(kLargeSize) * 4; i++) {
    int popped = queue->pop();
    if (popped < 0) {
      ASSERT_TRUE(popped < last_negative);
      last_negative = popped;
    } else {
      ASSERT_TRUE(popped > last_positive);
      last_positive = popped;
    }
  }
}

// Verify threaded operation
TEST_F(BlockingQueueTest, PushPopTwoThreads) {
  blocking_queue<int> queue(kLargeSize);
  thread thr1(std::bind(DoPop, &queue));
  thread thr2(std::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
 }

// Verify threaded operation with two writers.
TEST_F(BlockingQueueTest, PushPopThreeThreads) {
  blocking_queue<int> queue(kLargeSize);
  thread thr1(std::bind(DoPopPositiveOrNegative, &queue));
  thread thr2(std::bind(DoPushNegative, &queue));
  thread thr3(std::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
  thr3.join();
 }

static bool at_limit(const int limit, atomic<int> *value) {
  return limit <= value->fetch_add(1, memory_order_relaxed);
}

static void DoPopPositiveOrNegativeUntilLimit(blocking_queue<int> *queue,
                                              const int limit,
                                              atomic<int> *value) {
  int last_positive = -1;
  int last_negative = 0;
  // One thread is pushing increasing positive numbers, starting from 0, while
  // the other is pushing decreasing negative numbers starting from -1. Verify
  // that the popped number is increasing or decreasing as appropriate. (The
  // numbers will be interleaved.)
  while (!at_limit(limit, value)) {
    try {
      int popped = queue->pop();
      if (popped < 0) {
        ASSERT_TRUE(popped < last_negative);
        last_negative = popped;
      } else {
        ASSERT_TRUE(popped > last_positive);
        last_positive = popped;
      }
    } catch (gcl::closed_error expected) {
      // There are two threads reading from the queue. It's possible
      // that one thread may have grabbed multiple values, in which
      // case the queue will be closed for this thread. So ignore a
      // closed error.
    }
  }
}

// Verify threaded operation with two writers and two readers.
TEST_F(BlockingQueueTest, PushPopFourThreads) {
  blocking_queue<int> queue(kLargeSize);
  const int limit = static_cast<int>(kLargeSize) * 4;
  atomic<int>  num_popped;
  num_popped = 0;
  thread thr1(std::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr2(std::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr3(std::bind(DoPushNegative, &queue));
  thread thr4(std::bind(DoPush, &queue));
  thr3.join();
  thr4.join();
  queue.close();
  thr1.join();
  thr2.join();
  EXPECT_EQ(limit + 2, num_popped.load(memory_order_relaxed));
}
