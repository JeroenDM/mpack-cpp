#include "gtest/gtest.h"
#include "mpack_cpp/mpack_reader.hpp"

TEST(HelloTest, BasicAssertions) {
  EXPECT_EQ(mpack_cpp::add(3, 4), 7);
}