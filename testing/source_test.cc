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

// This file is a test of the source, using a buffer_queue.
//
// TODO(alasdair): This is a basic sanity check, and needs to be
// expanded.

#include "source.h"
#include "buffer_queue.h"

#include "gmock/gmock.h"

using gcl::source;
using gcl::buffer_queue;
using testing::_;
using testing::Invoke;
using testing::InSequence;

class SourceTest : public testing::Test {
};

TEST_F(SourceTest, Basic) {
  buffer_queue<int> queue(5);
  queue.push(42);

  source<int, buffer_queue<int> > test_source(&queue);
  ASSERT_FALSE(test_source.has_value());
  test_source.wait();
  ASSERT_TRUE(test_source.has_value());
  ASSERT_EQ(42, test_source.get());
}
