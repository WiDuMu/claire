#include <iostream>
#include "claire.hpp"

struct ServerConfig {
  bool enable_logging;

  // [[= field_enum::Description{"Show this help"}]]
  [[= claire::Description("This is stupid if it works") ]]
  bool help;
};

int main() {
  std::cout << claire::struct_fields<ServerConfig>();
  return 0;
}