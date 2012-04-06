// Copyright 2011 Google Inc. All Rights Reserved.
//
// Basic test of the mutable_thread class.

#include <stdio.h>

#include "atomic.h"
#include "called_task.h"
#include "condition_variable.h"
#include "mutable_thread.h"
#include "thread.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

namespace gcl {
namespace {

TEST(MutableThreadTest, TestExecute) {
  Called called(2);
  mutable_thread t;

  // Queue up 2 units of work. The first will run, but the second unit
  // will not complete until the count is incremented (by calling
  // wait).
  t.execute(tr1::bind(&Called::run, &called));
  t.execute(tr1::bind(&Called::wait, &called));

  // Short sleep just to give the threads some time to execute.
  this_thread::sleep_for(chrono::milliseconds(1));

  EXPECT_EQ(1, called.count.load());

  // Then release the thread by calling wait() and let the count go up.
  called.run();

  // Short sleep just to give the threads some time to execute.
  this_thread::sleep_for(chrono::milliseconds(1));

  // Count should go up to 2.
  EXPECT_EQ(2, called.count.load());
}

TEST(MutableThreadTest, TestJoin) {
  Called called(1);

  mutable_thread t;
  t.execute(tr1::bind(&Called::run, &called));
  called.wait();

  // Join should complete at this point.
  t.join();

  EXPECT_TRUE(t.is_done());
}
}
}  // namespace gcl
