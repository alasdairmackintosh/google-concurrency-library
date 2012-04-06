// Copyright 2010 Google Inc. All Rights Reserved.

// This file is a test of the source, using a
// blocking_queue.
//
// TODO(alasdair): This is a basic sanity check, and needs to be
// expanded.

#include "source.h"
#include "blocking_queue.h"

#include "gmock/gmock.h"

namespace tr1 = std::tr1;
using gcl::source;
using gcl::blocking_queue;
using testing::_;
using testing::Invoke;
using testing::InSequence;

class SourceTest : public testing::Test {
};

TEST_F(SourceTest, Basic) {
  blocking_queue<int> queue;
  queue.push(42);

  source<int, blocking_queue<int> > test_source(queue);
  ASSERT_FALSE(test_source.has_value());
  test_source.wait();
  ASSERT_TRUE(test_source.has_value());
  ASSERT_EQ(42, test_source.get());
}
