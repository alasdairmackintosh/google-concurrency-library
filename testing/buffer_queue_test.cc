// Copyright 2011 Google Inc. All Rights Reserved.

#include <tr1/functional>
#include <iostream>
#include "thread.h"

namespace tr1 = std::tr1;

#include "gmock/gmock.h"
using testing::_;
using testing::Invoke;
using testing::InSequence;

#include "atomic.h"
using std::atomic_int;
using std::memory_order_relaxed;

#include "buffer_queue.h"
using gcl::buffer_queue;
using gcl::queue_op_status;

inline std::ostream& operator<<(std::ostream& stream,
                                queue_op_status status)
{
  switch ( status ) {
    case queue_op_status::success: stream << "success"; break;
    case queue_op_status::empty: stream << "empty"; break;
    case queue_op_status::full: stream << "full"; break;
    case queue_op_status::closed: stream << "closed"; break;
    default: stream << "FAILURE";
  }
  return stream;
}

const size_t kSize = 3;
const size_t kLargeSize = 300;
const size_t kZero = 0;

class BufferQueueTest : public testing::Test {
 protected:
  // Pushes a standard set of numbers onto the queue
  void push_elements(buffer_queue<int>& queue) {
    ASSERT_TRUE(queue.is_empty());
    for(int i = 0; i < static_cast<int>(kSize); i++) {
      ASSERT_EQ(queue_op_status::success, queue.try_push(i));
      ASSERT_FALSE(queue.is_empty());
    }
  }

  // Pops a standard set of numbers from the queue, and validates the result.
  void pop_elements(buffer_queue<int>& queue) {
    for(int i = 0; i < static_cast<int>(kSize); i++) {
      int popped;
      ASSERT_FALSE(queue.is_empty());
      ASSERT_EQ(queue_op_status::success, queue.try_pop(popped));
      ASSERT_EQ(i, popped);
    }
    ASSERT_TRUE(queue.is_empty());
  }
};

// Verifies that we cannot create a queue of size zero
TEST_F(BufferQueueTest, InvalidArg0) {
  try {
    buffer_queue<int> queue(0);
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Verify basic push/pop operations.
TEST_F(BufferQueueTest, Single) {
  buffer_queue<int> queue(kSize);
  int popped;
  ASSERT_TRUE(queue.is_empty());
  ASSERT_EQ(queue_op_status::success, queue.try_push(8));
  ASSERT_FALSE(queue.is_empty());
  ASSERT_EQ(queue_op_status::success, queue.try_pop(popped));
  ASSERT_EQ(8, popped);
  ASSERT_TRUE(queue.is_empty());
}

// Verify basic push/pop operations.
TEST_F(BufferQueueTest, Simple) {
  buffer_queue<int> queue(kSize);
  push_elements(queue);
  pop_elements(queue);
}

// Verifies that we can create a queue from iterators.
TEST_F(BufferQueueTest, CreateFromIterators) {
  std::vector<int> values;
  values.push_back(0);
  values.push_back(1);
  values.push_back(2);
  ASSERT_EQ(kSize, values.size());
  buffer_queue<int> queue(values.size(), values.begin(), values.end());
  pop_elements(queue);
}

// Verifies that we cannot create a queue from iterators where the
// maximum size is less than that defined by the iterators
TEST_F(BufferQueueTest, InvalidIterators) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  try {
    buffer_queue<int> queue(2, values.begin(), values.end());
    FAIL();
  } catch (std::invalid_argument expected) {
  }
}

// Verify that try_pop fails when the queue is empty, but succeeds when a new
// element is added.
TEST_F(BufferQueueTest, TryPop) {
  buffer_queue<int> queue(kSize);
  int result;
  ASSERT_EQ(queue_op_status::empty, queue.try_pop(result));
  ASSERT_EQ(queue_op_status::success, queue.try_push(1));
  ASSERT_EQ(queue_op_status::success, queue.try_pop(result));
  ASSERT_EQ(1, result);
  ASSERT_TRUE(queue.is_empty());
}

// Verify that try_push succeeds until we exceed the size limit
TEST_F(BufferQueueTest, TryPush) {
  buffer_queue<int> queue(kSize);
  push_elements(queue);
  ASSERT_EQ(queue_op_status::full, queue.try_push(42));

  // Pop an element off the front. We should now be able to push.
  ASSERT_EQ(0, queue.pop());
  ASSERT_EQ(queue_op_status::success, queue.try_push(42));

  ASSERT_EQ(1, queue.pop());
  ASSERT_EQ(2, queue.pop());
  ASSERT_EQ(42, queue.pop());
  ASSERT_TRUE(queue.is_empty());
}

// Verify that we cannot try_push to a closed queue
// nor try_pop an empty closed queue
TEST_F(BufferQueueTest, TryPushPopClosed) {
  buffer_queue<int> queue(kSize);
  push_elements(queue);
  queue.close();
  ASSERT_TRUE(queue.is_closed());
  ASSERT_EQ(queue_op_status::closed, queue.try_push(42));
  pop_elements(queue);
  ASSERT_TRUE(queue.is_closed());
  int popped;
  ASSERT_EQ(queue_op_status::closed, queue.try_pop(popped));
}

// Verify that we cannot push to a closed queue
// nor pop from an empty closed queue
TEST_F(BufferQueueTest, PushPopClosed) {
  buffer_queue<int> queue(kSize);
  push_elements(queue);
  queue.close();
  ASSERT_TRUE(queue.is_closed());
  try {
    queue.push(42);
    FAIL();
  } catch (queue_op_status expected) {
    ASSERT_EQ(queue_op_status::closed, expected);
  }
  pop_elements(queue);
  ASSERT_TRUE(queue.is_closed());
  try {
    queue.pop();
    FAIL();
  } catch (queue_op_status expected) {
    ASSERT_EQ(queue_op_status::closed, expected);
  }
}

// The following tests push elements onto the queue from one thread, and read
// from another. They will test the wait/notify mechanism (and they show no
// errors when run with the ThreadSanitizer) but they are not deterministic.
// TODO(alasdair): add tests using new thread blocking mechanism.

static void DoPush(buffer_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    queue->push(i);
  }
}

static void DoPushNegative(buffer_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    queue->push(-1 - i);
  }
}

static void DoPop(buffer_queue<int> *queue) {
  for (int i = 0; i < static_cast<int>(kLargeSize) * 2; i++) {
    int popped = queue->pop();
    ASSERT_EQ(i, popped);
  }
}

#if 0

static void DoPopPositiveOrNegative(buffer_queue<int> *queue) {
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

#endif

// Verify threaded operation
TEST_F(BufferQueueTest, PushPopTwoThreads) {
  buffer_queue<int> queue(kLargeSize);
  thread thr1(tr1::bind(DoPop, &queue));
  thread thr2(tr1::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
 }

#if 0

// Verify threaded operation with two writers.
TEST_F(BufferQueueTest, PushPopThreeThreads) {
  buffer_queue<int> queue(kLargeSize);
  thread thr1(tr1::bind(DoPopPositiveOrNegative, &queue));
  thread thr2(tr1::bind(DoPushNegative, &queue));
  thread thr3(tr1::bind(DoPush, &queue));
  thr1.join();
  thr2.join();
  thr3.join();
 }

#endif

static bool at_limit(const int limit, atomic_int *value) {
  return limit <= value->fetch_add(1, memory_order_relaxed);
}

static void DoPopPositiveOrNegativeUntilLimit(buffer_queue<int> *queue,
                                              const int limit,
                                              atomic_int *value) {
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
    } catch (queue_op_status expected) {
      ASSERT_EQ(queue_op_status::closed, expected);
      // There are two threads reading from the queue. It's possible
      // that one thread may have grabbed multiple values, in which
      // case the queue will be closed for this thread. So ignore a
      // closed error.
    }
  }
}

// Verify threaded operation with two writers and two readers.
TEST_F(BufferQueueTest, PushPopFourThreads) {
  buffer_queue<int> queue(kLargeSize);
  const int limit = static_cast<int>(kLargeSize) * 4;
  atomic_int  num_popped;
  num_popped = 0;
  thread thr1(tr1::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr2(tr1::bind(DoPopPositiveOrNegativeUntilLimit,
                        &queue, limit, &num_popped));
  thread thr3(tr1::bind(DoPushNegative, &queue));
  thread thr4(tr1::bind(DoPush, &queue));
  thr3.join();
  thr4.join();
  queue.close();
  thr1.join();
  thr2.join();
  EXPECT_EQ(limit + 2, num_popped.load(memory_order_relaxed));
}
