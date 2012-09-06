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

// This file is a test of the iterator_queue class

#include <vector>

#include "functional.h"

#include "atomic.h"

#include "countdown_latch.h"
#include "thread.h"
#include "iterator_queue.h"

#include "gmock/gmock.h"

using testing::_;

using std::vector;
using gcl::iterator_queue;
using gcl::countdown_latch;

class IteratorQueueTest : public testing::Test {
};

typedef std::vector<int> int_vector;
typedef std::vector<countdown_latch*> latch_vector;

// Verifies that we can create a queue from a basic vector, and read
// its values.
TEST_F(IteratorQueueTest, BasicRead) {
  int_vector values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  values.push_back(4);
  iterator_queue<int_vector::const_iterator > queue(values.begin(), values.end());
  for(size_t i = 0; i < values.size(); i++) {
    ASSERT_FALSE(queue.is_closed());
    ASSERT_EQ(values[i], queue.pop());
  }
  ASSERT_TRUE(queue.is_closed());
  try {
    queue.pop();
    FAIL();
  } catch (gcl::closed_error expected) {
  }
}

static void GetAndBlock(iterator_queue<latch_vector::const_iterator>* queue,
                        countdown_latch* started) {
  while (!queue->is_closed()) {
    countdown_latch* latch = queue->pop();
    started->count_down();
    latch->wait();
  }
}

// Verifies that we can read from a queue in multiple threads
TEST_F(IteratorQueueTest, ThreadedRead) {
  latch_vector latches;
  for (int i = 0; i < 4; i++) {
    latches.push_back(new countdown_latch(1));
  }
  iterator_queue<latch_vector::const_iterator> queue(latches.begin(), latches.end());
  countdown_latch latch1(1);
  countdown_latch latch2(1);
  countdown_latch latch3(1);
  thread thread1(std::bind(GetAndBlock, &queue, &latch1));
  thread thread2(std::bind(GetAndBlock, &queue, &latch2));
  thread thread3(std::bind(GetAndBlock, &queue, &latch3));
  latch1.wait();
  latch2.wait();
  latch3.wait();
  // At this point the three threads have all popped a value from the
  // queue, and will be blocked in GetAndBlock. There should be one
  // value remaining. Pop that and verify that the queue is closed.
  ASSERT_FALSE(queue.is_closed());
  queue.pop();
  ASSERT_TRUE(queue.is_closed());

  // Unblock the first three threads. They should all find that the
  // queue is closed, and will terminate.
  for(size_t i = 0; i < 3; i++) {
    latches[i]->count_down();
  }
  thread1.join();
  thread2.join();
  thread3.join();
  latches[3]->count_down();
  for(size_t i = 0; i < 4; i++) {
    delete latches[i];
  }
}
