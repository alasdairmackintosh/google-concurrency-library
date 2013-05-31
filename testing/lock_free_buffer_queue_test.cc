// Copyright 2013 Google Inc. All Rights Reserved.
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

#include "lock_free_buffer_queue.h"
#include "queue_base_test.h"

using gcl::lock_free_buffer_queue;

const int kSmall = 4;
const int kLarge = 1000;

class LockFreeBufferQueueTest
:
    public testing::Test
{
};

// Verifies that we cannot create a queue of size zero
TEST_F(LockFreeBufferQueueTest, InvalidArg0) {
  try {
    lock_free_buffer_queue<int> body(0);
    FAIL();
  } catch (std::invalid_argument expected) {
  } catch (...) {
    FAIL();
  }
}

// Verify single try push/pop operations.
TEST_F(LockFreeBufferQueueTest, SingleTry) {
  lock_free_buffer_queue<int> q(1);
  seq_try_fill(1, 1, &q);
  seq_try_drain(1, 1, &q);
}

// Verify multiple try push/pop operations.
TEST_F(LockFreeBufferQueueTest, MultipleTry) {
  lock_free_buffer_queue<int> q(kSmall);
  seq_try_fill(kSmall, 1, &q);
  seq_try_drain(kSmall, 1, &q);
}

// Verifies that we can create a queue from iterators.
TEST_F(LockFreeBufferQueueTest, CreateFromIterators) {
  std::vector<int> values;
  for ( int i = 1; i <= kSmall; ++i )
    values.push_back(i);
  ASSERT_EQ(static_cast<size_t>(kSmall), values.size());
  lock_free_buffer_queue<int> q(values.size(), values.begin(), values.end());
  seq_try_drain(kSmall, 1, &q);
}

// Verifies that we cannot create a queue from iterators where the
// maximum size is less than that defined by the iterators
TEST_F(LockFreeBufferQueueTest, InvalidIterators) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  try {
    lock_free_buffer_queue<int> q(2, values.begin(), values.end());
    FAIL();
  } catch (std::invalid_argument expected) {
  } catch (...) {
  }
}

// Verify that try_pop fails when the queue is empty, but succeeds when a new
// element is added.
TEST_F(LockFreeBufferQueueTest, TryPopEmpty) {
  lock_free_buffer_queue<int> q(kSmall);
  seq_try_empty(&q);
}

// Verify that try_push succeeds until we exceed the size limit
TEST_F(LockFreeBufferQueueTest, TryPushFull) {
  lock_free_buffer_queue<int> q(kSmall);
  seq_try_full(kSmall, &q);
}
