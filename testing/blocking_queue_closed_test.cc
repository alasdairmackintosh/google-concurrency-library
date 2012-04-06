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

// This file is a test of the blocking_queue 'close' methods. It uses
// the ThreadMonitor mechism to test the behaviour whan a queue is
// closed, and variousn threads are blocking waiting to add or remove
// elements.

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "atomic.h"

#include "blocking_queue.h"
#include "countdown_latch.h"
#include "test_mutex.h"

#include "gmock/gmock.h"

using testing::_;
using testing::Invoke;
using testing::InSequence;

using gcl::blocking_queue;
using gcl::countdown_latch;

size_t kSize = 3;
size_t kLargeSize = 300;
size_t kZero = 0;

class BlockingQueueClosedTest : public testing::Test {
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

// Pushes the specified number of elements onto the queue. Expects the
// next push to throw a closed_error
static void PushUntilClosed(blocking_queue<int>* queue,
                            int count) {
  for (int i = 0; i < count; i++) {
    queue->push(i);
  }
  try {
    queue->push(0);
    FAIL();
  } catch (gcl::closed_error expected) {
  }
}

// Pops the specified number of elements from the queue. Expects the
// next pop to throw a closed_error
static void PopUntilClosed(blocking_queue<int>* queue,
                           int count) {
  for (int i = 0; i < count; i++) {
    queue->pop();
  }
  try {
    queue->pop();
    FAIL();
  } catch (gcl::closed_error expected) {
  }
}

// Creates a thread that fills the queue. Waits until that thread is
// blocked trying to push onto a full queue, and then closes the
// queue.
TEST_F(BlockingQueueClosedTest, CloseWhenWaitingToPush) {
  ThreadMonitor* monitor = ThreadMonitor::GetInstance();
  blocking_queue<int> queue(kLargeSize);
  thread push_thread(std::bind(PushUntilClosed, &queue, kLargeSize));
  // Wait until the thread is blocked waiting to push onto a full queue
  monitor->WaitUntilBlocked(push_thread.get_id());
  ASSERT_EQ(kLargeSize, queue.size());
  queue.close();
  ASSERT_TRUE(queue.is_closed());
  push_thread.join();
  // Queue should still be full.
  ASSERT_EQ(kLargeSize, queue.size());

  // Verify that we can still pop the existing elements.
  thread pop_thread(std::bind(PopUntilClosed, &queue, kLargeSize));
  pop_thread.join();
  // Queue should now be empty
  ASSERT_EQ(kZero, queue.size());
}

// Creates a thread that empties the queue. Waits until that thread is
// blocked trying to pop from an empty queue, and then closes the
// queue.
TEST_F(BlockingQueueClosedTest, CloseWhenWaitingToPop) {
  ThreadMonitor* monitor = ThreadMonitor::GetInstance();
  blocking_queue<int> queue(kLargeSize);
  for (int i = 0; i < static_cast<int>(kLargeSize); i++) {
    queue.push(i);
  }
  thread pop_thread(std::bind(PopUntilClosed, &queue, kLargeSize));
  // Wait until the thread is blocked waiting to pop onto a full queue
  monitor->WaitUntilBlocked(pop_thread.get_id());
  ASSERT_EQ(kZero, queue.size());
  queue.close();
  ASSERT_TRUE(queue.is_closed());
  pop_thread.join();
  // Queue should still be empty
  ASSERT_EQ(kZero, queue.size());

  // We should not ba able to push anything onto a closed queue.
  PushUntilClosed(&queue, 0);
}
