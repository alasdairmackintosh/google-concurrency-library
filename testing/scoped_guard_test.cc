#include "scoped_guard.h"

#include "gtest/gtest.h"

using namespace gcl;

int i;

class ScopedGuardTest : public testing::Test {
 protected:
  void SetUp() {
    i = 0;
  }

};

void inc_i() {
  i++;
}

class incrementer {
 public:
  void operator()() {
    inc_i();
  }
};

void scoped_no_inc() {
  scoped_guard g(inc_i);
  g.dismiss();
}

TEST_F(ScopedGuardTest, Basics) {
  EXPECT_EQ(i, 0);
  {
    scoped_guard g(inc_i);
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, 1);
  {
    scoped_guard g(inc_i);
    EXPECT_EQ(i, 1);
    g.dismiss();
    EXPECT_EQ(i, 1);
  }
  EXPECT_EQ(i, 1);
}

TEST_F(ScopedGuardTest, Callable) {
  incrementer c;
  EXPECT_EQ(i, 0);
  {
    scoped_guard g(c);
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, 1);
  {
    scoped_guard g(c);
    EXPECT_EQ(i, 1);
    g.dismiss();
    EXPECT_EQ(i, 1);
  }
  EXPECT_EQ(i, 1);
}

template<typename T>
scoped_guard make_guard(T t) {
  scoped_guard s(t);
  return s;
}

#ifdef HAS_CXX11_RVREF
TEST_F(ScopedGuardTest, Factory) {
  EXPECT_EQ(i, 0);
  {
    auto g = make_guard(inc_i);
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, 1);
  {
    auto g = make_guard(inc_i);
    EXPECT_EQ(i, 1);
    g.dismiss();
    EXPECT_EQ(i, 1);
  }
  EXPECT_EQ(i, 1);

  {
    scoped_guard j(inc_i);
    scoped_guard k(inc_i);
    EXPECT_EQ(i, 1);
    j = std::move(k); // Old j is destroyed.
    EXPECT_EQ(i, 2);
    j = std::move(j); // Self move is no-op.
    EXPECT_EQ(i, 2);
    scoped_guard l(std::move(j));
    EXPECT_EQ(i, 2);
  }
  EXPECT_EQ(i, 3);
}
#endif


