#pragma once

#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

struct MockMega65Fixture : public ::testing::Test {
 protected:
  MockMega65Fixture() : conn(false) {}
  mock::MockMega65 conn;
};

}  // namespace m65dap::test
