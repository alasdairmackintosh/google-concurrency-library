// Copyright 2011 Google Inc. All Rights Reserved.
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

#include "buffer_queue.h"
#include "queue_base_test.h"

using gcl::buffer_queue;

const int kSmall = 4;
const int kLarge = 1000;


class BufferQueueTest
:
    public testing::Test
{
};

// Verifies that we cannot create a queue of size zero
TEST_F(BufferQueueTest, InvalidArg0) {
  try {
    buffer_queue<int> body(0);
    FAIL();
  } catch (std::invalid_argument expected) {
  } catch (...) {
    FAIL();
  }
}

// Verify single push/pop operations.
TEST_F(BufferQueueTest, Single) {
  buffer_queue<int> body(1, "body");
  seq_fill(1, 1, &body);
  seq_drain(1, 1, &body);
}

// Verify single try push/pop operations.
TEST_F(BufferQueueTest, SingleTry) {
  buffer_queue<int> body(1, "body");
  seq_try_fill(1, 1, &body);
  seq_try_drain(1, 1, &body);
}

// Verify multiple push/pop operations.
TEST_F(BufferQueueTest, Multiple) {
  buffer_queue<int> body(kSmall, "body");
  seq_fill(kSmall, 1, &body);
  seq_drain(kSmall, 1, &body);
}

// Verify multiple try push/pop operations.
TEST_F(BufferQueueTest, MultipleTry) {
  buffer_queue<int> body(kSmall, "body");
  seq_try_fill(kSmall, 1, &body);
  seq_try_drain(kSmall, 1, &body);
}

// Verifies that we can create a queue from iterators.
TEST_F(BufferQueueTest, CreateFromIterators) {
  std::vector<int> values;
  for ( int i = 1; i <= kSmall; ++i )
    values.push_back(i);
  ASSERT_EQ(static_cast<size_t>(kSmall), values.size());
  buffer_queue<int> body(values.size(), values.begin(), values.end(), "body");
  seq_drain(kSmall, 1, &body);
}

// Verifies that we cannot create a queue from iterators where the
// maximum size is less than that defined by the iterators
TEST_F(BufferQueueTest, InvalidIterators) {
  std::vector<int> values;
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  try {
    buffer_queue<int> body(2, values.begin(), values.end(), "body");
    FAIL();
  } catch (std::invalid_argument expected) {
  } catch (...) {
  }
}

// Verify that try_pop fails when the queue is empty, but succeeds when a new
// element is added.
TEST_F(BufferQueueTest, TryPopEmpty) {
  buffer_queue<int> body(kSmall, "body");
  seq_try_empty(&body, &body);
}

// Verify that try_push succeeds until we exceed the size limit
TEST_F(BufferQueueTest, TryPushFull) {
  buffer_queue<int> body(kSmall, "body");
  seq_try_full(kSmall, &body, &body);
}

// Verify that we cannot push to a closed queue
// nor pop from an empty closed queue
TEST_F(BufferQueueTest, PushPopClosed) {
  buffer_queue<int> body(kSmall, "body");
  seq_push_pop_closed(kSmall, &body, &body);
}

// Verify that we cannot try_push to a closed queue
// nor try_pop an empty closed queue
TEST_F(BufferQueueTest, TryPushPopClosed) {
  buffer_queue<int> body(kSmall, "body");
  seq_try_push_pop_closed(kSmall, &body, &body);
}

// Verify sequential producer consumer queue.
TEST_F(BufferQueueTest, SeqProdCom) {
  buffer_queue<int> body(kSmall, "body");
  seq_producer_consumer(kSmall, body);
}

// Verify producer consumer queue.
TEST_F(BufferQueueTest, ProdCom) {
  buffer_queue<int> body(kSmall, "body");
  producer_consumer(kLarge, body);
}

// Verify try producer consumer queue.
TEST_F(BufferQueueTest, TryProdCom) {
  buffer_queue<int> body(kSmall, "body");
  try_producer_consumer(kLarge, body);
}

// Verify sequential filtering pipes.
TEST_F(BufferQueueTest, SeqPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  seq_pipe(kSmall, head, tail);
}

// Verify linear filtering pipes
TEST_F(BufferQueueTest, LinearPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  linear_pipe(kLarge, head, tail);
}

// Verify linear filtering try pipes
TEST_F(BufferQueueTest, LinearTryPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  linear_try_pipe(kLarge, head, tail);
}

// Verify merging filtering pipes
TEST_F(BufferQueueTest, MergingPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  merging_pipe(kLarge, head, tail);
}

// Verify merging filtering try pipes
TEST_F(BufferQueueTest, MergingTryPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  merging_try_pipe(kLarge, head, tail);
}

// Verify parallel filtering pipes
TEST_F(BufferQueueTest, ParallelPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  parallel_pipe(kLarge, head, tail);
}

// Verify parallel filtering mixed pipes
TEST_F(BufferQueueTest, ParallelMixedPipe) {
  buffer_queue<int> head(kSmall, "head");
  buffer_queue<int> tail(kSmall, "tail");
  parallel_mixed_pipe(kLarge, head, tail);
}
