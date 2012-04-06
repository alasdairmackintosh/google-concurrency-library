// Tests thread behaviour using the test condition variable. Verifies
// that the test framework allows us to block until a thread is
// waiting on a condition variable.

#include "test_mutex.h"
#include "thread.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

static const int kNumThreads = 1;

static void WaitOnConditionVariable(mutex& mu,
                                    condition_variable& cv,
                                    bool* ready) {
  unique_lock<mutex> lock(mu);
  cv.wait(lock);
  *ready = true;
}

// Verifies that WaitUntilBlocked will wait until a thread is waiitng
// on a condition_variable
TEST(ConditionVariableTest, WaitUntilBlocked) {
  bool ready = false;
  mutex mu;
  condition_variable cv;
  thread thr(tr1::bind(
      WaitOnConditionVariable, tr1::ref(mu), tr1::ref(cv), &ready));
  ThreadMonitor::GetInstance()->WaitUntilBlocked(thr.get_id());
  EXPECT_FALSE(ready);
  mu.lock();
  cv.notify_one();
  mu.unlock();
  thr.join();
  EXPECT_TRUE(ready);
}

// Verifies that WaitUntilBlocked will wait until a thread is waiting
// on a condition_variable
TEST(ConditionVariableTest, WaitUntilBlockedMultiThreaded) {
  mutex mu;
  condition_variable cv;
  thread* threads[kNumThreads];
  bool ready[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    ready[i] = false;
    threads[i] = new thread(tr1::bind(WaitOnConditionVariable,
                                      tr1::ref(mu),
                                      tr1::ref(cv),
                                      ready+ i));
  }
  ThreadMonitor* monitor = ThreadMonitor::GetInstance();
  for (int i = 0; i < kNumThreads; i++) {
    monitor->WaitUntilBlocked(threads[i]->get_id());
    EXPECT_FALSE(ready[i]);
  }
  mu.lock();
  cv.notify_all();
  mu.unlock();
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
    delete threads[i];
    EXPECT_TRUE(ready[i]);
  }
}
