#ifndef CLAIRE_HELPERS_HPP
#define CLAIRE_HELPERS_HPP

#include <cstddef>
#include <cstring>
namespace claire {

template<std::meta::info i, typename T>
consteval bool same_type_as() {
   if (!std::meta::is_type(i)) {
      return std::meta::type_of(i) == ^^T;
   }
   return i == ^^T;
}

template<const char* short_name>
inline bool check_short_flag(const char* argument) {
   return strcmp(short_name, argument + 1) == 0;
}

template <const char* short_name>
inline const char* check_optional_arg(const char* argument) {
   constexpr std::size_t len = std::strlen(short_name);
   if (strncmp(short_name, argument + 1, len) == 0) {
      return argument + len;
   }

   return nullptr;
}

}

#endif // CLAIRE_HELPERS_HPP