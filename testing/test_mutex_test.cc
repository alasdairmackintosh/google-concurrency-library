// Tests thread behaviour using the test mutex classes. Verifies that
// the test framework allows us to block until a thread reaches a
// particular state.

#include "test_mutex.h"
#include "thread.h"
#include "mutex.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

static const int kNumThreads = 100;

// Sets 'id' to the id of this thread.
void SetThreadId(thread::id* id) {
  *id = this_thread::get_id();
}

static void LockMutex(mutex& mu, bool* ready) {
  mu.lock();
  *ready = true;
  mu.unlock();
}

// Verifies that wait_until_blocked will wait until a thread reaches
TEST(ThreadTest, WaitUntilBlocked) {
  mutex mu;
  mu.lock();
  bool ready = false;
  thread thr(tr1::bind(LockMutex, tr1::ref(mu), &ready));
  ThreadMonitor::GetInstance()->WaitUntilBlocked(thr.get_id());
  EXPECT_FALSE(ready);
  mu.unlock();
  thr.join();
  EXPECT_TRUE(ready);
}

TEST(ThreadTest, ManyThreads) {
  mutex mu;
  mu.lock();
  thread* threads[kNumThreads];
  bool ready[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    ready[i] = false;
    threads[i] = new thread(tr1::bind(LockMutex, tr1::ref(mu), ready + i));
  }
  mu.unlock();
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->join();
    EXPECT_TRUE(ready[i]);
    delete threads[i];
  }
}
