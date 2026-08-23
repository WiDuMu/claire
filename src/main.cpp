#include "claire.hpp"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>

enum GreetingStyles { Normal, Italic, Bold, BoldItalic };

struct[[= claire::Description("Revolutionary helpstring printer")]]
    ServerConfig {
  [[= claire::Description("Name of the person to greet")]] std::string name;

  [[= claire::Description("Number of times to repeat greeting")]] int num;

  [[= claire::Description("file to use")]] std::optional<std::filesystem::path>
      file;

  [[ = claire::Description("Verbose logging"),= claire::Shortname("v") ]]
  bool verbose;

  [[= claire::Description("Style of text to use"), = claire::Positional()]]
  std::optional<GreetingStyles> style;
};

int main(int argc, const char** argv) {
  constexpr std::string_view help_string =
      claire::create_help_string<ServerConfig>();
  auto cfg = claire::parse_args<ServerConfig>(argc, argv);
  if (!cfg.has_value()) {
    std::cout << help_string << cfg.error();
    return 1;
  }

  std::string greeting = "Hello ";

  if (cfg->verbose) { std::cout << "Verbose logging enabled" << "\n"; }

  if (cfg->style.has_value()) {
    std::cout << "Style: " << (long)cfg->style.value() << '\n';
  }

  for (int i = 0; i < cfg->num; i++) {
    std::cout << greeting << cfg->name << '\n';
  }
  return 0;
}
