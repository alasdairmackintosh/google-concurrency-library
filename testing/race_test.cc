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

// This file is a test for races and race detectors.
// TODO: extend this test:
//  - Add more tests with races.
//  - Add tests on which race detectors may potentially give false warnings.
//  - Add tests that use dynamic annotations.

#include "functional.h"

#include "thread.h"

#include "test_mutex.h"

#include "gmock/gmock.h"

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
  thread thr1(std::bind(IncrementArg1, &racey));
  thread thr2(std::bind(IncrementArg1, &racey));
  thr1.join();
  thr2.join();
  EXPECT_EQ(racey, 2);  // With a tiny probability this will fail.
}

// Two racey accesses, lock/unlock between them.
// Pure happens-before detectors will not find this race.
TEST(ThreadTest, DataRaceWithLockInBetween) {
  int racey = 0;
  mutex mu;
  thread thr1(std::bind(IncrementArg1LockAndUnlockArg2, &racey, &mu));
  thread thr2(std::bind(SleepLockAndUnlockArg2IncrementArg1, &racey, &mu));
  thr1.join();
  thr2.join();
  EXPECT_EQ(racey, 2);  // With a tiny probability this will fail.
}
