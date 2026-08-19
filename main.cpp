#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include "claire.hpp"

enum GreetingStyles {
    Normal,
    Italic,
    Bold,
    BoldItalic
};

struct
[[= claire::Description("Revolutionary helpstring printer")]]
ServerConfig {
  [[= claire::Description("Name of the person to greet")]]
  std::string name;

  [[= claire::Description("Number of times to repeat greeting") ]]
  int num;

  [[= claire::Description("file to use")]]
  std::optional<std::filesystem::path> file;

  [[= claire::Description("Verbose logging"), = claire::Shortname("v")]]
  bool verbose;

  [[= claire::Description("Style of text to use")]]
  std::optional<GreetingStyles> style;
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
    std::cout << claire::create_help_string<ServerConfig>() << cfg.error();
    return 1;
  }

  std::string greeting = "Hello ";

  if (cfg->verbose) {
    std::cout << "Verbose logging enabled" << "\n";
  }

  if (cfg->style.has_value()) {
      std::cout << "Style: " << (long) cfg->style.value() << '\n';
  }

  for (int i = 0; i < cfg->num; i++) {
    std::cout << greeting << cfg->name << '\n';
  }
  return 0;
}
