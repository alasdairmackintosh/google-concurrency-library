// This file is a rudimentary test of the thread class.  It primarily
// serves to make sure the build system works.

#include "thread.h"
#include "mutex.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

void WaitForThenSet(mutex& mu, bool* ready, bool* signal) {
  lock_guard<mutex> l(mu);
  while (!*ready) {
    mu.unlock();
    this_thread::sleep_for(chrono::milliseconds(10));
    mu.lock();
  }
  *signal = true;
}

TEST(ThreadTest, StartsNewThread) {
  bool ready = false;
  bool signal = false;
  mutex mu;
  thread thr(tr1::bind(WaitForThenSet, tr1::ref(mu), &ready, &signal));
  lock_guard<mutex> l(mu);
  EXPECT_FALSE(signal);
  ready = true;
  while (!signal) {
    mu.unlock();
    this_thread::sleep_for(chrono::milliseconds(10));
    mu.lock();
  }
  thr.detach();
}

TEST(ThreadTest, JoinSynchronizes) {
  bool ready = true;
  bool signal = false;
  mutex mu;
  thread thr(tr1::bind(WaitForThenSet, tr1::ref(mu), &ready, &signal));
  thr.join();
  EXPECT_TRUE(signal);
}
