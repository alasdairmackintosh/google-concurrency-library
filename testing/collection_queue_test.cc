// Copyright 2010 Google Inc. All Rights Reserved.

// This file is a test of the collection_queue class
#include "collection_queue.h"

#include "atomic.h"
#include "countdown_latch.h"

#include "gmock/gmock.h"
#include <functional>
#include <vector>

namespace tr1 = std::tr1;
using testing::_;

using std::vector;
using gcl::collection_queue;
using gcl::countdown_latch;

class CollectionQueueTest : public testing::Test {
};

// Verifies that we can create a queue from a basic vector, and read
// its values.
TEST_F(CollectionQueueTest, BasicRead) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  values.push_back(4);
  collection_queue<vector<int> > queue(values);
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

static void GetAndBlock(collection_queue<vector<countdown_latch*> >* queue,
                        countdown_latch* started) {
  while (!queue->is_closed()) {
    countdown_latch* latch = queue->pop();
    started->count_down();
    latch->wait();
  }
}

// Verifies that we can read from a queue in multiple threads
TEST_F(CollectionQueueTest, ThreadedRead) {
  std::vector<countdown_latch*> latches;
  for (int i = 0; i < 4; i++) {
    latches.push_back(new countdown_latch(1));
  }
  collection_queue<vector<countdown_latch*> > queue(latches);
  countdown_latch latch1(1);
  countdown_latch latch2(1);
  countdown_latch latch3(1);
  thread thread1(tr1::bind(GetAndBlock, &queue, &latch1));
  thread thread2(tr1::bind(GetAndBlock, &queue, &latch2));
  thread thread3(tr1::bind(GetAndBlock, &queue, &latch3));
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
  for(size_t i = 0; i < 4; i++) {
    delete latches[i];
  }
}
