#include <cstddef>
#include <iostream>
#include <optional>
#include "claire.hpp"

struct
[[= claire::Description("Revolutionary helpstring printer")]]
ServerConfig {
  [[= claire::Description("Name of the person to greet")]]
  std::string name;

  [[= claire::Description("Number of times to repeat greeting") ]]
  std::size_t num;

  // [[= claire::Description("Custom greeting to use") ]]
  std::optional<std::string> greeting;

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
    return 1;
  }

  std::string greeting = "Hello ";

  // if (cfg->greeting.has_value()) {
  //   greeting = cfg->greeting.value();
  // }

  if (cfg->verbose) {
    std::cout << "Verbose logging enabled" << "\n";
  }

  for (int i = 0; i < cfg->num; i++) {
    std::cout << greeting << cfg->name << '\n';
  }
  return 0;
}