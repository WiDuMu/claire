#include <iostream>
#include "claire.hpp"

struct
[[= claire::Description("Revolutionary helpstring printer")]]
ServerConfig {
  // double lolnoob;

  std::string path;

  [[= claire::Description("This is stupid if it works") ]]
  unsigned short num;

  [[= claire::Description("Verbose logging"), = claire::Shortname("v")]]
  bool verbose;
};

template <typename T>
void print_argument_values(const T& args) {
  constexpr static auto fields = claire::get_fields<T>();
  std::string s;

  template for (constexpr auto field : fields) {
    constexpr auto name = field.long_name;
    typename [: field.type :] val = fields.[: field.val :];
    s += name;
    s += ": ";
    s += val;
    s += '\n';
  }

  std::cout << s;
}

int main(int argc, const char** argv) {
  auto cfg = claire::parse_args<ServerConfig>(argc, argv);
  if (!cfg.has_value()) {
    std::cout << claire::struct_fields<ServerConfig>();
  } else {
    std::cout << "Path: " << cfg->path << '\n';
    std::cout << "Print help: " << cfg->num << '\n';
  }
  return 0;
}