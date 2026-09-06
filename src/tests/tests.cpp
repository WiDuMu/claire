#include <gtest/gtest.h>
#include "claire.hpp"

TEST(HelloTest, FloatParsing) {
    std::optional<float> result;
    float val;
  ASSERT_NO_THROW(result = claire::parse_arg<float>("1.0"));
  ASSERT_TRUE(result.has_value());
  ASSERT_NO_THROW(val = *result);
  EXPECT_EQ(val, 1.0f);
}

enum Colors {
    Black,
    Red,
    Blue,
    Green,
    Teal
};

TEST(HelloTest, EnumParsing) {
    std::optional<Colors> result;
    ASSERT_NO_THROW(result = claire::parse_arg<Colors>("black"));
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(Colors::Black, *result);
}

TEST(HelloTest, EnumParsingFails) {
    std::optional<Colors> result;
    ASSERT_NO_THROW(result = claire::parse_arg<Colors>("potato"));
    ASSERT_FALSE(result.has_value());
}

struct Test1 {
    bool verbose;
    Colors color;
    std::string path;
};

TEST(HelloTest, ArgParsing) {
    std::expected<Test1, const char*> result;
    Test1 val;
    std::array<const char*, 4> args = {"claire_test", "--verbose", "teal", "src"};
    ASSERT_NO_THROW(result = claire::parse_args<Test1>(args.size(), args.data()));
  ASSERT_TRUE(result.has_value());
  ASSERT_NO_THROW(val = *result);
  EXPECT_TRUE(val.verbose);
  EXPECT_EQ(val.path, "src");
  EXPECT_EQ(Colors::Teal, val.color);
}

TEST(HelloTest, NotEnoughParameters) {
    std::expected<Test1, const char*> result;
    Test1 val;
    std::array<const char*, 2> args = {"claire_test", "--verbose"};
    ASSERT_NO_THROW(result = claire::parse_args<Test1>(args.size(), args.data()));
  ASSERT_FALSE(result.has_value());
}

struct Test2 {
    std::optional<std::string> c;
    int i;
};

TEST(HelloTest, IncompleteParameters) {
    std::expected<Test2, const char*> result;
    Test2 val;
    auto args = std::array{"claire_test", "--c", "--verbose", "1"};
    ASSERT_NO_THROW(result = claire::parse_args<Test2>(args.size(), args.data()));
  EXPECT_FALSE(result.has_value()) << result.error();
}
