#include <gtest/gtest.h>
#include <filesystem>
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

// TEST(HelloTest, IncompleteParameters) {
//     std::expected<Test2, const char*> result;
//     Test2 val;
//     auto args = std::array{"claire_test", "--c", "--verbose", "1"};
//     ASSERT_NO_THROW(result = claire::parse_args<Test2>(args.size(), args.data()));
//   EXPECT_FALSE(result.has_value()) << result.error();
// }

struct Test4 {
  char a;
  short b;
  int c;
  long d;
  long long int e;
  unsigned char f;
  unsigned short g;
  unsigned int h;
  unsigned long i;
  unsigned long long int j;
  // <float> k;
  // <double> l;
  std::string m;
  std::string_view n;
  std::filesystem::path o;
};

TEST(HelloTest, ParsersExist) {
  std::vector<const char*> argj = {"programname", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "/src"};
  std::expected<Test4, const char*> result;
  ASSERT_NO_THROW(result = claire::parse_args<Test4>(argj.size(), argj.data()));
  ASSERT_TRUE(result) << result.error();
  Test4 v = *result;
  ASSERT_EQ(v.b, 1);
}

struct Test3 {
  std::optional<char> a;
  std::optional<short> b;
  std::optional<int> c;
  std::optional<long> d;
  std::optional<long long int> e;
  std::optional<unsigned char> f;
  std::optional<unsigned short> g;
  std::optional<unsigned int> h;
  std::optional<unsigned long> i;
  std::optional<unsigned long long int> j;
  // std::optional<float> k;
  // std::optional<double> l;
  std::optional<std::string> m;
  std::optional<std::string_view> n;
  std::optional<std::filesystem::path> o;
  int num;
};

TEST(HelloTest, OptionalParsersExist) {
  std::vector<const char*> argj = {
      "programname",
      "--a",
      "1",
      "--b",
      "1",
      "--c",
      "1",
      "--d",
      "1",
      "--e",
      "1",
      "--f",
      "1",
      "--g",
      "1",
      "--h",
      "1",
      "--i",
      "1",
      "--j",
      "1",
      "--m",
      "str",
      "--n",
      "str",
      "--o",
      "/src",
      "0"
  };
  std::expected<Test3, const char*> result;
  ASSERT_NO_THROW(result = claire::parse_args<Test3>(argj.size(), argj.data()));
  ASSERT_TRUE(result) << result.error()  << '\n' << claire::create_help_string<Test3>();
  Test3& v = *result;
  ASSERT_EQ(v.a, 1);
}
