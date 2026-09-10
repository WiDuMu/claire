/*-----------------------------------------------------------------------------+
|                                                                              |
|       Claire: Command Line Arguments Interpretation Reflection Engine        |
|                             (C) WiDuMu 2026                                  |
|                                  0.2.1                                       |
|                                                                              |
+-----------------------------------------------------------------------------*/

// This library implements a command line arguments parser similar to rust's
// `clap`'s derive functionality. You provide a struct, and it generates a
// helpstring, and argument parser for your struct type.
//
// WARNING: Currently parse_args returns a std::expected<T, const char*>
// For errors that can be predicted at compile time, the error is a static
// string. For error messages that have to be generated at runtime, the data is
// owned by a thread-local internal object that can be overwritten by future
// calls to parse_args. Be careful with lifetime management.
//
// USAGE:
// #include <iostream>
//
// #include "claire.hpp"
//
// using claire::Description, claire::Shortname;
//
// struct
// [[= Description("Greeting generator")]]
// Args {
//    [[= Description("Name to greet")]]
//    std::string name;
//    [[= Description("Print this help string"), = Shortname("h")]]
//    bool help;
//    [[= Description("Print version"), = Shortname("V")]]
//    bool version;
// };
//
// int main(int argc, const char** argv) {
//    auto args_result = claire::parse_args<Args>(argc, argv);
//
//    if (!args_result) {
//       std::cerr << args_result.error();
//       return 1;
//    }
//
//    auto args = *args_result;
//
//    if (args.version) {
//       std::cout << "Greeting 1.0\n";
//       return 0;
//    }
//
//    if (args.help) {
//       std::cout << claire::create_help_string<Args>();
//       return 0;
//    }
//
//    std::cout << "Hello, " << args.name << "\n";
// }
//
// Which should output:
// $ ./greet hello -h
// Greeting generator
//
// USAGE: <name>
//    name Name to greet
// Options:
//    -h --help Print this help string
//    -V --version Print version

//
// Current To-Do List:
// - [X] Optional positionals
// - [ ] --arg=whatever handling
// - [ ] Partial number consumption flag (accept/don't)
// - [ ] Allocation reduction
// - [ ] Bad Allocation exception errors
// - [ ] Better Error Handling (struct type that doesn't alloc unless asked for)
// - [ ] Vector/Array types, which allow/require a given number of paramters
// - [ ] Better unicode support
// - [ ] Repeated argument detector.
// - [ ] Case insensitve enum matching
// - [ ] Bypassing flags (flags which bypass positional requirements, i.e.
// --help)
// - [X] Unit Testing
// - [ ] Subcommands
// - [X] Proper enum types

// List for clarification of position:
// - [ ] Clustered short flags (this is incompatible with current paradigm,
// which allows for short flags to be more than one character, making clustered
// flags ambigous. However, create_help_string's formatting a short name is 1
// character long.)

#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <expected>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace claire {

/*---------------------------------------------------------------------------+
|                                                                            |
|                                  Structs                                   |
|                                                                            |
+---------------------------------------------------------------------------*/

// WARNING: THIS FUNCTIONALITY IS BROKEN IN CURRENT BUILDS, IT IS AWAITING A
// PARSER REWRITE.
//
/// Marks an optional argument as a "bypassing" argument.
/// When one of these are detected, parsing ends as soon as the flag is
/// detected. To avoid unintended behavior, bypassing arugments should be
/// checked first.
///
/// This option is intended for flags that cause the program to ignore the rest
/// of the arguments, so verifying that the rest of arguments parse correctly is
/// counter productive, e.g. --help or --version
///
/// #TODO figure out how to implement this without a second parsing pass
struct Bypass {};

/// A description of a parameter
/// Usage:
/// struct Args {
///    [[= Description("The username to use to log in")]]
///    const char* user_name;
/// }
struct Description {
  const char* text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  [[nodiscard]] consteval Description(const std::string_view txt) noexcept
      : text(std::define_static_string(txt)) {}
};

// Marks an optional field as a positional optional
// It will have positional semantics, but will be parsed as an optional value,
// and not error if not specified
struct Positional {};

/// A optional short name for a parameter
/// Usage:
/// struct Args {
///    [[= Shortname("U")]]
///    const char* user_name;
/// }
struct Shortname {
  const char* text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  [[nodiscard]] consteval Shortname(const std::string_view txt) noexcept
      : text(std::define_static_string(txt)) {}
};

/// Which pass this argument needs to be processed on
enum ParsePass { Position, Option, OptionalPosition, OptionalBypass };

// Claire implementation details not for public consumption
namespace impl {
using std::define_static_string;
using std::expected;
using std::string;
using std::unexpected;
using std::meta::identifier_of;
using std::meta::info;
using std::meta::nonstatic_data_members_of;
using std::meta::type_of;
using constr = const char* const;

/*---------------------------------------------------------------------------+
|                                                                            |
|                                   Types                                    |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Internal structure used to store details of each argument provided
struct ArgumentDeets {
  const char* long_name;
  const char* short_name;
  const char* description;
  info type;
  info val;
  ParsePass pass;
};

/// Internal enum used to check if a optional parser matched
/// This could be a bool but I found the semantics difficult when used with a
/// std::optional
enum MatchStatus { NotMatched, Matched };

/// Internal enum used to specify if parse_optionals found a positional argument
/// or ran out first.
enum PositionalStatus { NotFound, Found };

/*---------------------------------------------------------------------------+
|                                                                            |
|                           Library statics                                  |
|                                                                            |
+---------------------------------------------------------------------------*/

// Access context used by reflection functions
constexpr inline auto context = std::meta::access_context::current();

// This is a static variable that stores heap-allocated error strings.
thread_local inline std::string err_return_msg;

/*---------------------------------------------------------------------------+
|                                                                            |
|                              Helper functions                              |
|                                                                            |
+---------------------------------------------------------------------------*/

// This is a quick and dirty way of getting a tolower function to work
// in a constexpr context.
// #TODO Unicode handling, though that is much more complicated.
[[nodiscard]] constexpr char ascii_tolower(const char c) noexcept {
  if (c >= 'A' && c <= 'Z') return c + 32;
  return c;
}

// Runs over the string view provided and returns a new string of it lowercase.
[[nodiscard]] constexpr std::string
ascii_tolower(const std::string_view v) noexcept {
  std::string s{v};
  std::transform(s.begin(), s.end(), s.begin(),
                 [](const char c) { return ascii_tolower(c); });
  return s;
}

/// Extract the 'text' field from an annotated struct
template <std::meta::info i, typename T>
[[nodiscard]] consteval const char* extract_text_annotation() noexcept {
  constexpr static auto shortnames = std::define_static_array(
      std::meta::annotations_of_with_type(i, ^^const T));

  template for (constexpr auto name : shortnames) {
    constexpr const char* txt = std::meta::extract<const T>(name).text;
    if (txt) { return txt; }
  }

  return std::define_static_string("");
}

/// Check if the given value has an annotation of the type given
template <std::meta::info i, typename T>
[[nodiscard]] consteval bool has_annotation_of_type() noexcept {
  constexpr static auto annotations_length =
      std::meta::annotations_of_with_type(i, ^^const T).size();
  return annotations_length > 0;
}

template <typename T>
[[nodiscard]] constexpr inline std::expected<T, const char*>
unknown_argument(const char* arg) {
  err_return_msg = "Error: unknown argument: ";
  err_return_msg += arg;
  err_return_msg += '\n';
  return std::unexpected(err_return_msg.c_str());
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Type Concepts                                |
|                                                                            |
+---------------------------------------------------------------------------*/

[[nodiscard]] consteval inline bool is_optional_type(info type) noexcept {
  return std::meta::has_template_arguments(type) &&
         (std::meta::template_of(type) == ^^std::optional);
}

template <typename T>
[[nodiscard]] consteval inline bool is_optional_type() noexcept {
  return is_optional_type(^^T);
}

/// Optional arguments are either boolean and assumed to be a flag
/// or wrapped in a std::optional
template <std::meta::info type>
[[nodiscard]] consteval bool is_optional() noexcept {
  if (!std::meta::is_type(type)) { return false; }

  if (type == ^^bool) { return true; }

  return is_optional_type(type);
}

template <typename T>
  requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
[[nodiscard]] constexpr std::optional<T> parse_numeric(const char* str) noexcept {
  if (!str) { return std::nullopt; }
  size_t len = std::strlen(str);
  T val;
  auto result = std::from_chars(str, str + len, val);
  if (!result) { return std::nullopt; }
  if (result.ptr != (str + len)) {
      return std::nullopt;
  } else {
      return val;
  }
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Value parsers                                |
|                                                                            |
+---------------------------------------------------------------------------*/


/// If a argument is a boolean type, if it exists at all it is true
template <typename T>
  requires std::same_as<T, bool>
[[nodiscard]] constexpr inline std::optional<bool>
parse_arg([[maybe_unused]] const char* str) noexcept {
  return true;
}

/// Specialization of generic function parse_arg for enum types
template <typename T>
  requires std::is_enum_v<T>
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  static_assert(std::meta::is_enumerable_type(^^T), "Requires an enum");
  constexpr static auto enum_members =
      std::define_static_array(std::meta::enumerators_of(^^T));

  if (!str) { return std::nullopt; }

  template for (constexpr auto member : enum_members) {
    constexpr auto display_name = std::meta::display_string_of(member);
    constexpr auto cli_name =
        std::define_static_string(impl::ascii_tolower(display_name));

    if (strcmp(cli_name, str) == 0) {
      constexpr T val = [:member:];
      return val;
    }
  }
  return std::nullopt;
}

/// Specialization of generic function parse_arg for integer types
template <typename T>
  requires(std::integral<T>) && (!std::same_as<T, bool>)
[[nodiscard]] constexpr inline std::optional<T> parse_arg(const char* str) noexcept {
  return parse_numeric<T>(str);
}

/// Specialization of generic function parse_arg for floating point types
/// not constexpr compatible because from_chars is not constexpr compatible for
/// floating point types
template <typename T>
  requires(std::floating_point<T>) && (!std::same_as<T, bool>)
[[nodiscard]] inline std::optional<T> parse_arg(const char* str) noexcept {
  return parse_numeric<T>(str);
}

/// Generic form of parse_arg for types that can be constructed from strings
template <typename T>
  requires std::constructible_from<T, const char*> &&
           (!std::same_as<T, bool>) && (!is_optional_type<T>())
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  if (!str) { return std::nullopt; }
  try {
    return T{str};
  } catch (...) { return std::nullopt; }
}

// For an optional type
template <typename T>
  requires(is_optional_type<T>())
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  using R = T::value_type;
  auto val = parse_arg<R>(str);
  if (val.has_value()) { return val.value(); }
  return std::nullopt;
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                             parse_args helpers                             |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Checks if a C string is not empty
[[nodiscard]] constexpr inline bool not_emptystring(const char* s) noexcept {
  return s && s[0] != '\0';
}

/// Gets all fields of a struct, and creates a static array of the details
template <typename T>
[[nodiscard]] constexpr auto get_fields() noexcept {
  std::vector<ArgumentDeets> fields{};

  constexpr auto static members =
      define_static_array(nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    constexpr info member_type = type_of(member);
    const char* member_name = define_static_string(identifier_of(member));
    const char* member_desc = extract_text_annotation<member, Description>();
    const char* member_short_name =
        extract_text_annotation<member, Shortname>();
    bool opt = is_optional<member_type>();
    ParsePass pass = opt ? Option : Position;
    if (opt) {
      constexpr bool pos = has_annotation_of_type<member, Positional>();
      constexpr bool bypass = has_annotation_of_type<member, Bypass>();
      static_assert(!(pos && bypass),
                    "Cannot be both bypass and positional at the same time");
      if (pos) {
        pass = OptionalPosition;
      } else if (bypass) {
        pass = OptionalBypass;
      }
    }
    ArgumentDeets deets{
        member_name, member_short_name, member_desc, member_type, member, pass};
    fields.push_back(deets);
  }

  return std::define_static_array(fields);
}

/// Filters the fields to only return the options that are parsed during a given
/// pass #TODO This is inefficient
template <typename T, ParsePass pass>
[[nodiscard]] constexpr auto get_pass_fields() noexcept {
  constexpr static auto fields = get_fields<T>();
  std::vector<ArgumentDeets> val;

  for (auto field : fields) {
    if (field.pass == pass) { val.push_back(field); }
  }

  return define_static_array(val);
}

template <typename T, ArgumentDeets deets, size_t offset, const char* name>
[[nodiscard]] constexpr inline expected<MatchStatus, const char*>
parse_optional(T& ret, int const argc, int& argp, const char**& argv) noexcept {
  constexpr constr err_parsing_msg = define_static_string(
      string{"Error: failed to parse argument '"} + name + "'\n");
  constexpr constr err_missing_msg = define_static_string(
      string{"Error: missing value for argument '"} + name + "'\n");

  // If we don't match, bail
  if (strcmp(name, argv[argp] + offset)) { return NotMatched; }

  if constexpr (deets.type == ^^bool) {
    ret.[:deets.val:] = true;
    return Matched;
  }

  if ((argp + 1) >= argc) { return unexpected(err_missing_msg); }

  ++argp; // #TODO: add in = handling to args. i.e. --file=filename
  auto result = parse_arg<typename[:deets.type:]>(argv[argp]);
  if (result) {
    ret.[:deets.val:] = result.value();
    return Matched;
  }
  return unexpected(err_parsing_msg);
}

template <typename T>
[[nodiscard]] constexpr inline expected<PositionalStatus, const char*>
parse_optionals(T& ret, int const argc, int& argp,
                const char**& argv) noexcept {
  constexpr static auto optionals = get_pass_fields<T, Option>();

  for (; argp < argc; argp++) {
    bool unknown_arg = true;
    const char* arg = argv[argp];

    // Is it a optional argument?
    if (arg[0] == '-') {

      // Short optional arugment
      if (arg[1] != '\0' && arg[1] != '-') {

        template for (constexpr auto option : optionals) {

          if constexpr (not_emptystring(option.short_name)) {
            auto result = parse_optional<T, option, 1, option.short_name>(
                ret, argc, argp, argv);
            if (!result.has_value()) { return unexpected(result.error()); }
            if (*result == Matched) {
              unknown_arg = false;
              break;
            }
          }
        }

        if (unknown_arg) { return unknown_argument<PositionalStatus>(arg); }

      } else if (arg[1] == '-' && arg[2] != '\0') { // Long flag
        template for (constexpr auto option : optionals) {
          auto result = parse_optional<T, option, 2, option.long_name>(
              ret, argc, argp, argv);
          if (!result.has_value()) { return unexpected(result.error()); }
          if (*result == Matched) {
            unknown_arg = false;
            break;
          }
        }

        if (unknown_arg) { return unknown_argument<PositionalStatus>(arg); }
      }
    } else {
      return Found;
    }
  }
  return NotFound;
}

template <typename T, ArgumentDeets deets>
[[nodiscard]] constexpr inline expected<void, const char*>
parse_positional(T& ret, int const argc, int& argp,
                 const char**& argv) noexcept {
  constexpr constr err_parsing_string = define_static_string(
      string{"Error: failed parsing argument "} + deets.long_name + '\n');
  constexpr constr err_not_exists_string = define_static_string(
      string{"Error: missing value for argument "} + deets.long_name + '\n');

  auto optional_result = parse_optionals<T>(ret, argc, argp, argv);

  if (!optional_result.has_value()) {
    return unexpected(optional_result.error());
  }

  if (argp >= argc) { return unexpected(err_not_exists_string); }

  auto val = parse_arg<typename[:deets.type:]>(argv[argp]);

  if (val.has_value()) {
    argp++;
    ret.[:deets.val:] = *val;
    return {};
  }

  return unexpected(err_parsing_string);
}

template <typename T, ArgumentDeets deets>
[[nodiscard]] constexpr inline expected<PositionalStatus, const char*>
parse_optional_positional(T& ret, int const argc, int& argp,
                          const char**& argv) noexcept {
  constexpr constr err_parsing_string = define_static_string(
      string{"Error: failed parsing argument "} + deets.long_name + '\n');

  auto optional_result = parse_optionals<T>(ret, argc, argp, argv);

  if (!optional_result.has_value()) {
    return unexpected(optional_result.error());
  }

  if (argp >= argc) { return NotFound; }

  auto val = parse_arg<typename[:deets.type:]>(argv[argp]);

  if (val.has_value()) {
    argp++;
    ret.[:deets.val:] = *val;
    return Found;
  }

  return unexpected(err_parsing_string);
}

} // namespace impl

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Main functions                               |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Generate a help string for your arguments
template <typename T>
  requires std::is_class_v<T>
[[nodiscard]] consteval const char* create_help_string() {
  constexpr auto program_desc =
      impl::extract_text_annotation<^^T, Description>();
  std::string s;

  if (impl::not_emptystring(program_desc)) {
    s += program_desc;
    s += "\n\n";
  }

  // #TODO handle optional positionals here
  constexpr auto static positionals = impl::get_pass_fields<T, Position>();
  constexpr auto static optionals = impl::get_pass_fields<T, Option>();
  constexpr auto static optionalPositionals =
      impl::get_pass_fields<T, OptionalPosition>();

  if (positionals.size()) {
    s += "USAGE:";

    template for (constexpr auto field : positionals) {
      s += " <";
      s += field.long_name;
      s += ">";
    }

    template for (constexpr auto field : optionalPositionals) {
      s += " [";
      s += field.long_name;
      s += "]";
    }

    s += '\n';
  }

  template for (constexpr auto field : positionals) {
    if (impl::not_emptystring(field.description)) {
      s += "   ";
      s += field.long_name;
      s += " ";
      s += field.description;
      s += "\n";
    }
  }

  template for (constexpr auto field : optionalPositionals) {
    if (impl::not_emptystring(field.description)) {
      s += "   ";
      s += field.long_name;
      s += " ";
      s += field.description;
      s += "\n";
    }
  }

  if (optionals.size()) { s += "Options:\n"; }

  template for (constexpr auto field : optionals) {
    s += "   ";
    if (field.short_name && field.short_name[0] != '\0') {
      s += "-";
      s += field.short_name;
    } else {
      s += "  ";
    }
    s += " --";
    s += field.long_name;
    s += " ";
    s += field.description;
    s += "\n";
  }

  return std::define_static_string(s);
}

/// Parse arguments into the provided struct type
template <typename T>
  requires std::is_class_v<T>
[[nodiscard]] constexpr std::expected<T, const char*>
parse_args(int argc, const char** argv) {
  constexpr static auto positionals = impl::get_pass_fields<T, Position>();
  constexpr static auto optional_positionals =
      impl::get_pass_fields<T, OptionalPosition>();
  T ret{};
  int argp = 1;

  // Iterate through the positional fields of the struct
  // Iterate through the arguments, if we find a argument that isn't a flag
  // i.e. `--verbose`, or a optional i.e. `--logging-level verbose`, break,
  // process the positional, and continue.
  template for (constexpr auto field : positionals) {
    auto positional_result =
        impl::parse_positional<T, field>(ret, argc, argp, argv);

    if (!positional_result.has_value()) {
      return std::unexpected(positional_result.error());
    }
  }

  template for (constexpr auto opt_pos : optional_positionals) {
    auto optional_positional_result =
        impl::parse_optional_positional<T, opt_pos>(ret, argc, argp, argv);

    if (!optional_positional_result.has_value()) {
      return std::unexpected(optional_positional_result.error());
    }

    if (optional_positional_result.value() == impl::NotFound) { break; }
  }

  if (argp < argc) { // More optionals exist
    auto optional_result = impl::parse_optionals<T>(ret, argc, argp, argv);
    if (!optional_result.has_value()) {
      return std::unexpected(optional_result.error());
    }
  }

  if (argp < argc) { // We encountered an unexpected positional argument
    return impl::unknown_argument<T>(argv[argp]);
  }

  return ret;
}

} // namespace claire

#endif // CLAIRE_HPP
