#include <gtest/gtest.h>

#include "opcodes.h"

namespace m65dap::test {

TEST(OpcodesSuite, GetBsrOpcodes)
{
  static constexpr auto result = get_opcodes(Mnemonic::BSR);
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result.front().mnemonic, Mnemonic::BSR);
  EXPECT_EQ(result.front().code, std::byte{0x63});
}

TEST(OpcodesSuite, GetJsrOpcodes)
{
  static constexpr auto result = get_opcodes(Mnemonic::JSR);
  EXPECT_EQ(result.size(), 3);
  for (const auto& o : result) {
    EXPECT_EQ(o.mnemonic, Mnemonic::JSR);
  }
}

TEST(OpcodesSuite, NumOpcodes)
{
  static constexpr auto num_jsr = get_num_opcodes({Mnemonic::JSR});
  EXPECT_EQ(num_jsr, 3);
  static constexpr auto num_bsr = get_num_opcodes(Mnemonic::BSR);
  EXPECT_EQ(num_bsr, 1);
  static constexpr auto num_all = get_num_opcodes({Mnemonic::JSR, Mnemonic::BSR});
  EXPECT_EQ(num_all, num_bsr + num_jsr);
}

}  // namespace m65dap::test
