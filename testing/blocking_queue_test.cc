// Copyright 2010 Google Inc. All Rights Reserved.

// This file is a test of the blocking_queue class
#include "blocking_queue.h"
#include "test_mutex.h"

#include "gmock/gmock.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace tr1 = std::tr1;
using testing::_;
using testing::Invoke;
using testing::InSequence;

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
  thread thr1(tr1::bind(DoPop, &queue));
  thread thr2(tr1::bind(DoPush, &queue));
  THREAD_DBG << "Created thr1 " << thr1.get_id() << ENDL;
  THREAD_DBG << "Created thr2 " << thr2.get_id() << ENDL;
  thr1.join();
  thr2.join();
 }

// Verify threaded operation with two writers.
TEST_F(BlockingQueueTest, PushPopThreeThreads) {
  blocking_queue<int> queue(kLargeSize);
  thread thr1(tr1::bind(DoPopPositiveOrNegative, &queue));
  thread thr2(tr1::bind(DoPushNegative, &queue));
  thread thr3(tr1::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
  thr3.join();
 }

// TODO(alasdair): Use atomics when available
static mutex limit_mutex;
static bool at_limit(const int limit, int *value) {
  unique_lock<mutex> l(limit_mutex);
  return limit <= (*value)++;
}

static void DoPopPositiveOrNegativeUntilLimit(blocking_queue<int> *queue,
                                              const int limit,
                                              int *value) {
  int last_positive = -1;
  int last_negative = 0;
  // One thread is pushing increasing positive numbers, starting from 0, while
  // the other is pushing decreasing negative numbers starting from -1. Verify
  // that the popped number is increasing or decreasing as appropriate. (The
  // numbers will be interleaved.)
  while (!at_limit(limit, value)) {
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

// Verify threaded operation with two writers and two readers.
TEST_F(BlockingQueueTest, PushPopFourThreads) {
  blocking_queue<int> queue(kLargeSize);
  const int limit = static_cast<int>(kLargeSize) * 4;
  int  num_popped = 0;
  thread thr1(tr1::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr2(tr1::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr3(tr1::bind(DoPushNegative, &queue));
  thread thr4(tr1::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
  thr3.join();
  thr4.join();
 }
