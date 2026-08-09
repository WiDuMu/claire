#ifndef CLAIRE_PARSE_ARG_HPP
#define CLAIRE_PARSE_ARG_HPP

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>

template<typename T>
inline std::optional<T> parse_numeric(const char *str) {
   size_t len = std::strlen(str);
   T val;
   auto result = std::from_chars(str, str + len, val);
   if (result) {
      return val;
   }
   return std::nullopt;
}

template<typename T>
requires std::integral<T> || std::floating_point<T>
std::optional<T> parse_arg(const char *str) {
   return parse_numeric<T>(str);
}

template <typename T> std::optional<T> parse_arg(const char *str) {
  try {
    return T{str};
  } catch (...) {
    return std::nullopt;
  }
}

template <> std::optional<bool> parse_arg<bool>(const char *str) {
  return true;
}

template <> std::optional<std::string> parse_arg<std::string>(const char *str) {
  try {
    return std::string{str};
  } catch (...) {
    return std::nullopt;
  }
}

#endif // CLAIRE_PARSE_ARG_HPP