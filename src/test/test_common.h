#pragma once

#include "mock_mega65.h"

namespace m65dap::test {

void test_command(mock::MockMega65& conn, std::string_view cmd, const std::vector<std::string>& expected_lines);

}
