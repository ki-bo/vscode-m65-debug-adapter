#include "util.h"

#include <gtest/gtest.h>

namespace m65dap::test {

TEST(UtilSuite, ReplaceAllTest)
{
  std::string str{"The quick brown fox jumps over the lazy dog."};
  str = replace_all(str, "brown", "red");
  EXPECT_EQ(str, "The quick red fox jumps over the lazy dog.");
}

TEST(UtilSuite, ReplaceAllWithTemporaryTest)
{
  std::string str{"The quick brown fox jumps over the lazy dog."};
  EXPECT_EQ(replace_all(str, "brown", "red"), "The quick red fox jumps over the lazy dog.");
  EXPECT_EQ(str, "The quick red fox jumps over the lazy dog.");
}

TEST(UtilSuite, ReplaceAllMultipleMatchesTest)
{
  std::string str{"The quick brown fox jumps over the lazy fox."};
  str = replace_all(str, "fox", "rabbit");
  EXPECT_EQ(str, "The quick brown rabbit jumps over the lazy rabbit.");
}

TEST(UtilSuite, ReplaceAllBeginningAndEndTest)
{
  std::string str{"The quick brown fox jumps over the lazy dog.The"};
  str = replace_all(str, "The", "A");
  EXPECT_EQ(str, "A quick brown fox jumps over the lazy dog.A");
}

TEST(UtilSuite, ReplaceAllNoMatchTest)
{
  std::string str{"The quick brown fox jumps over the lazy dog."};
  str = replace_all(str, "rabbit", "cat");
  EXPECT_EQ(str, "The quick brown fox jumps over the lazy dog.");
}

}  // namespace m65dap::test
