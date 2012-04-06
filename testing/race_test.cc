// This file is a test for races and race detectors.
// TODO: extend this test:
//  - Add more tests with races.
//  - Add tests on which race detectors may potentially give false warnings.
//  - Add tests that use dynamic annotations.

#include "thread.h"
#include "mutex.h"

#include "gmock/gmock.h"
#include <tr1/functional>

namespace tr1 = std::tr1;

static void IncrementArg1(int *arg) {
  (*arg)++;
}

static void IncrementArg1LockAndUnlockArg2(int *arg1, mutex *mu) {
  (*arg1)++;
  mu->lock();
  mu->unlock();
}

static void SleepLockAndUnlockArg2IncrementArg1(int *arg1, mutex *mu) {
  this_thread::sleep_for(chrono::milliseconds(1000));
  mu->lock();
  mu->unlock();
  (*arg1)++;
}

// Simple race.
TEST(ThreadTest, SimpleDataRaceTest) {
  int racey = 0;
  thread thr1(tr1::bind(IncrementArg1, &racey));
  thread thr2(tr1::bind(IncrementArg1, &racey));
  thr1.join();
  thr2.join();
  EXPECT_EQ(racey, 2);  // With a tiny probability this will fail.
}

// Two racey accesses, lock/unlock between them.
// Pure happens-before detectors will not find this race.
TEST(ThreadTest, DataRaceWithLockInBetween) {
  int racey = 0;
  mutex mu;
  thread thr1(tr1::bind(IncrementArg1LockAndUnlockArg2, &racey, &mu));
  thread thr2(tr1::bind(SleepLockAndUnlockArg2IncrementArg1, &racey, &mu));
  thr1.join();
  thr2.join();
  EXPECT_EQ(racey, 2);  // With a tiny probability this will fail.
}
