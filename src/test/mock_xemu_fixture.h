#pragma once

#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

struct MockXemuFixture : public ::testing::Test {
 protected:
  MockXemuFixture() : conn(true) {}
  mock::MockMega65 conn;
};

}  // namespace m65dap::test
