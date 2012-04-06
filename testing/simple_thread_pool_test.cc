// Copyright 2011 Google Inc. All Rights Reserved.

// Basic test of the simple_thread_pool class.

#include <stdio.h>

#include "atomic.h"
#include "called_task.h"
#include "condition_variable.h"
#include "mutable_thread.h"
#include "simple_thread_pool.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

namespace gcl {
namespace {

TEST(SimpleThreadPoolTest, GetOne) {
  simple_thread_pool thread_pool;
  mutable_thread* new_thread = thread_pool.try_get_unused_thread();
  EXPECT_FALSE(NULL == new_thread);

  Called called(1);
  new_thread->execute(tr1::bind(&Called::run, &called));
  called.wait();
  EXPECT_EQ(1, called.count);
}

TEST(SimpleThreadPoolTest, GetAndReturnOne) {
  simple_thread_pool thread_pool;
  mutable_thread* new_thread = thread_pool.try_get_unused_thread();
  EXPECT_FALSE(NULL == new_thread);

  Called called(1);
  new_thread->execute(tr1::bind(&Called::run, &called));
  called.wait();
  EXPECT_EQ(1, called.count);

  EXPECT_TRUE(thread_pool.donate_thread(new_thread));
  // Thread is now inactive, so it can't be donated again.
  EXPECT_FALSE(thread_pool.donate_thread(new_thread));
}

TEST(SimpleThreadPoolTest, MultiExecute) {
  simple_thread_pool thread_pool;
  int num_threads = 10;

  Called called(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    mutable_thread* new_thread = thread_pool.try_get_unused_thread();
    EXPECT_FALSE(NULL == new_thread);
    new_thread->execute(tr1::bind(&Called::run, &called));
  }
  called.wait();
  EXPECT_EQ(10, called.count);
}

TEST(SimpleThreadPoolTest, OutOfThreads) {
  int num_threads = 10;
  simple_thread_pool thread_pool(0, num_threads - 1);

  Called called(num_threads);
  mutable_thread* a_thread = NULL;
  for (int i = 0; i < (num_threads - 1); ++i) {
    mutable_thread* new_thread = thread_pool.try_get_unused_thread();
    EXPECT_FALSE(NULL == new_thread);
    new_thread->execute(tr1::bind(&Called::run, &called));

    if (a_thread == NULL) {
      a_thread = new_thread;
    }
  }
  mutable_thread* new_thread = thread_pool.try_get_unused_thread();
  EXPECT_EQ(NULL, new_thread);
  
  // Now try to release a thread and return it to the unused pile.
  while (called.count < (num_threads - 1)) {
    // wait
  }

  thread_pool.donate_thread(a_thread);
  new_thread = thread_pool.try_get_unused_thread();
  EXPECT_FALSE(NULL == new_thread);
  new_thread->execute(tr1::bind(&Called::run, &called));

  // Should now be able to wait for one last execution
  called.wait();
  EXPECT_EQ(num_threads, called.count);
}

}
}  // namespace gcl
