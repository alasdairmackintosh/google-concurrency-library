// This file is a rudimentary test of the lock classes.

#include "mutex.h"

#include "gmock/gmock.h"
#include <string>

using ::testing::InSequence;

class CheckPoint {
public:
  MOCK_METHOD1(Check, void(const std::string& id));
};

class MockMutex {
public:
  MOCK_METHOD0(lock, void());
  MOCK_METHOD0(unlock, void());
};

TEST(LockGuardTest, Simple) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  lock_guard<MockMutex> l(mu);
  cp.Check("locked");
}

TEST(LockGuardTest, Adopt) {
  CheckPoint cp;
  MockMutex mu;
  {
    InSequence s;
    EXPECT_CALL(mu, lock());
    EXPECT_CALL(cp, Check("locked"));
    EXPECT_CALL(mu, unlock());
  }

  mu.lock();
  lock_guard<MockMutex> l(mu, adopt_lock);
  cp.Check("locked");
}
