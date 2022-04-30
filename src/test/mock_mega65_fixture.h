#pragma once

#include <gtest/gtest.h>

#include "mock_mega65.h"

namespace m65dap::test {

struct Mega65Fixture : public ::testing::Test {
 protected:
  mock::MockMega65 conn;
};

}  // namespace m65dap::test
