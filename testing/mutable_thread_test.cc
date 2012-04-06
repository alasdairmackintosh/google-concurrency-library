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

// Basic test of the mutable_thread class.

#include <stdio.h>

#include "functional.h"

#include "atomic.h"
#include "condition_variable.h"
#include "thread.h"

#include "called_task.h"
#include "mutable_thread.h"

#include "gmock/gmock.h"

namespace gcl {
namespace {

//using std::bind;

TEST(MutableThreadTest, TestExecute) {
  Called called(2);
  mutable_thread t;

  // Queue up 2 units of work, though first unit will not complete until the
  // count is incremented by the test thread (wait blocks).
  t.execute(std::bind(&Called::run, &called));
  t.execute(std::bind(&Called::wait, &called));
  // This call should block until the run command completes since the queue is
  // only 2 entries deep.
  t.execute(std::bind(&Called::run, &called));
  EXPECT_EQ(1, called.count.load());

  // Then release the thread by calling wait() and let the count go up.
  called.run();

  this_thread::sleep_for(chrono::milliseconds(1));

  // Count should go up to 3 (the 2 queued calls and the one run() call here).
  EXPECT_EQ(3, called.count.load());
}

TEST(MutableThreadTest, TestJoin) {
  Called called(1);

  mutable_thread t;
  t.execute(std::bind(&Called::run, &called));
  called.wait();

  // Join should complete at this point.
  t.join();

  EXPECT_TRUE(t.is_done());
}

}
}  // namespace gcl
