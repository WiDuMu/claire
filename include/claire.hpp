/*-----------------------------------------------------------------------------+
|                                                                              |
|       Claire: Command Line Arguments Interpretation Reflection Engine        |
|                             (C) WiDuMu 2026                                  |
|                                                                              |
+-----------------------------------------------------------------------------*/

#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include <algorithm>
#include <cassert>
#include <cctype>
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

/*---------------------------------------------------------------------------+
|                                                                            |
|                                  Structs                                   |
|                                                                            |
+---------------------------------------------------------------------------*/

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
  template <std::meta::info i>
  [[nodiscard]] consteval static const char* extract() noexcept {
    return extract_text_annotation<i, Description>();
  }
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
  template <std::meta::info i>
  [[nodiscard]] consteval static const char* extract() noexcept {
    return extract_text_annotation<i, Shortname>();
  }
};

/// Which pass this argument needs to be processed on
enum ParsePass { Position, Option, OptionalPosition, OptionalBypass };

/// Internal structure used to store details of each argument provided
struct ArgumentDeets {
  const char* long_name;
  const char* short_name;
  const char* description;
  std::meta::info type;
  std::meta::info val;
  ParsePass pass;
};

/*---------------------------------------------------------------------------+
|                                                                            |
|                           Library statics                                  |
|                                                                            |
+---------------------------------------------------------------------------*/

// This is a static variable that stores heap-allocated error strings.
// #TODO more testing to see if this results in effective use-after-frees due
// to modifying the string that was returned to the program.
inline std::string err_return_msg;

/*---------------------------------------------------------------------------+
|                                                                            |
|                              Helper functions                              |
|                                                                            |
+---------------------------------------------------------------------------*/

// This is a quick and dirty way of getting a tolower function to work
// in a constexpr context.
// #TODO Unicode handling, thought that is much more complicated.
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

/// Checks if a std::meta::info represents a type that is the same as T
template <std::meta::info i, typename T>
[[nodiscard]] consteval bool same_type_as() noexcept {
  if (!std::meta::is_type(i)) { return std::meta::type_of(i) == ^^T; }
  return i == ^^T;
}

/// Checks if a C string is not empty
[[nodiscard]] constexpr inline bool not_emptystring(const char* s) noexcept {
  return s && s[0] != '\0';
}

/// Optional arguments are either boolean and assumed to be a flag
/// or wrapped in a std::optional
template <std::meta::info type>
[[nodiscard]] consteval bool is_optional() noexcept {
  if (!std::meta::is_type(type)) { return false; }

  if (type == ^^bool) { return true; }

  if (!std::meta::has_template_arguments(type)) { return false; }

  return std::meta::template_of(type) == ^^std::optional;
}

/// Gets all fields of a struct, and creates a static array of the details
template <typename T>
[[nodiscard]] constexpr auto get_fields() noexcept {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    constexpr std::meta::info member_type = std::meta::type_of(member);
    const char* member_name =
        std::define_static_string(std::meta::identifier_of(member));
    const char* member_desc = Description::extract<member>();
    const char* member_short_name = Shortname::extract<member>();
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

  return std::define_static_array(val);
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Type Concepts                                |
|                                                                            |
+---------------------------------------------------------------------------*/

template <typename T>
struct is_optional_enum : std::false_type {};

template <typename E>
  requires std::is_enum_v<E>
struct is_optional_enum<std::optional<E>> : std::true_type {};

/// Is an enum stored inside a std::optional?
/// #TODO generalize this to any type constructable using parse_arg stored
/// inside std::optional
template <typename T>
concept OptionalEnum = is_optional_enum<T>::value;

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
        std::define_static_string(ascii_tolower(display_name));

    if (strcmp(cli_name, str) == 0) {
      constexpr T val = [:member:];
      return val;
    }
  }
  return std::nullopt;
}

// For an optional containing an enum
template <OptionalEnum T>
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  using EnumT = T::value_type;
  auto val = parse_arg<EnumT>(str);
  if (val.has_value()) { return val.value(); }
  return std::nullopt;
}

/// Specialization of generic function parse_arg for numeric types
/// Uses parse_numeric
template <typename T>
  requires(std::integral<T> || std::floating_point<T>) &&
          (!std::same_as<T, bool>)
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  if (!str) { return std::nullopt; }
  size_t len = std::strlen(str);
  T val;
  auto result = std::from_chars(str, str + len, val);
  if (result) { return val; }
  return std::nullopt;
}

/// Generic form of parse_arg for types that can be constructed from strings
template <typename T>
  requires std::constructible_from<T, const char*> && (!std::same_as<T, bool>)
[[nodiscard]] constexpr std::optional<T> parse_arg(const char* str) noexcept {
  if (!str) { return std::nullopt; }
  try {
    return T{str};
  } catch (...) { return std::nullopt; }
}

template <typename T, ArgumentDeets deets, std::size_t offset, const char* name>
[[nodiscard]] inline std::expected<bool, const char*>
parse_optional(T& ret, int const argc, int& argp, const char**& argv) noexcept {
  constexpr const char* const err_parsing_msg = std::define_static_string(
      std::string{"Error: failed to parse argument '"} + name + "'\n");
  constexpr const char* const err_missing_msg = std::define_static_string(
      std::string{"Error: missing value for argument '"} + name + "'\n");

  // If we don't match, bail
  if (strcmp(name, argv[argp] + offset)) { return false; }

  if constexpr (same_type_as<deets.type, bool>()) {
    ret.[:deets.val:] = true;
    return true;
  }

  if ((argp + 1) >= argc) { return std::unexpected(err_missing_msg); }

  ++argp; // #TODO: add in = handling to args. i.e. --file=filename
  auto result = parse_arg<typename[:deets.type:]>(argv[argp]);
  if (result) {
    ret.[:deets.val:] = result.value();
    return true;
  }
  return std::unexpected(err_parsing_msg);
}

template <typename T>
[[nodiscard]] inline std::expected<bool, const char*>
parse_optionals(T& ret, int const argc, int& argp,
                const char**& argv) noexcept {
  constexpr static auto optionals = get_pass_fields<T, Option>();

  if (!argv) { return std::unexpected("Error: argv is null?"); }

  for (; argp < argc; argp++) {
    const char* arg = argv[argp];

    // Is it a optional argument?
    if (arg[0] == '-') {

      // Short optional arugment
      if (arg[1] != '\0' && arg[1] != '-') {

        template for (constexpr auto option : optionals) {

          if constexpr (not_emptystring(option.short_name)) {
            auto result = parse_optional<T, option, 1, option.short_name>(
                ret, argc, argp, argv);
            if (!result.has_value()) { return std::unexpected(result.error()); }
            if (*result) {
              goto end_of_loop; // #TODO: Fix this without goto
            }
          }
        }

        err_return_msg = "Error: Unknown short argument: ";
        err_return_msg += arg;
        err_return_msg += '\n';

        return std::unexpected(err_return_msg.c_str());
      } else if (arg[1] == '-' && arg[2] != '\0') { // Long flag
        template for (constexpr auto option : optionals) {
          auto result = parse_optional<T, option, 2, option.long_name>(
              ret, argc, argp, argv);
          if (!result.has_value()) { return std::unexpected(result.error()); }
          if (*result) {
            goto end_of_loop; // #TODO: Fix this without goto
          }
        }

        err_return_msg = "Error: Unknown long argument: ";
        err_return_msg += arg;
        err_return_msg += '\n';

        return std::unexpected(err_return_msg.c_str());
      }
    } else {
      return true;
    }
  end_of_loop:
  }
  return false;
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Main functions                               |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Generate a help string for your arguments
template <typename T>
  requires std::is_class_v<T>
[[nodiscard]] consteval const char* create_help_string() {
  std::string s;

  constexpr auto program_desc = Description::extract<^^T>();

  if (not_emptystring(program_desc)) {
    s += program_desc;
    s += "\n\n";
  }

  constexpr auto static positionals = get_pass_fields<T, Position>();
  constexpr auto static optionals = get_pass_fields<T, Option>();

  if (positionals.size()) {
    s += "USAGE:";

    template for (constexpr auto field : positionals) {
      s += " <";
      s += field.long_name;
      s += ">";
    }

    s += '\n';
  }

  template for (constexpr auto field : positionals) {
    if (not_emptystring(field.description)) {
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
      s += " ";
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
  constexpr static auto positionals = get_pass_fields<T, Position>();
  constexpr static auto optional_positionals =
      get_pass_fields<T, OptionalPosition>();
  T ret{};
  int argp = 1;

  // Iterate through the positional fields of the struct
  // Iterate through the argugments, if we find a argument that isn't a flag
  // i.e. `--verbose`, or a optional i.e. `--logging-level verbose`, break,
  // process the positional, and continue. #TODO: currently a flag doesn't
  // process correctly
  template for (constexpr auto field : positionals) {
    constexpr const char* const err_parsing_string = std::define_static_string(
        std::string{"Error: Failed parsing argument "} + field.long_name +
        '\n');
    constexpr const char* const err_not_exists_string =
        std::define_static_string(
            std::string{"Error: Missing value for argument "} +
            field.long_name + '\n');

    auto optional_result = parse_optionals<T>(ret, argc, argp, argv);

    if (!optional_result.has_value()) {
      return std::unexpected(optional_result.error());
    }

    if (argp < argc) { // Positional argument exists
      std::optional<typename[:field.type:]> val =
          parse_arg<typename[:field.type:]>(argv[argp]);
      if (val.has_value()) {
        argp++;
        ret.[:field.val:] = *val;
      } else {
        return std::unexpected(err_parsing_string);
      }
    } else {
      return std::unexpected(err_not_exists_string);
    }
  }

  template for (constexpr auto opt_pos : optional_positionals) {
    constexpr const char* const err_parsing_string = std::define_static_string(
        std::string{"Error: Failed parsing argument "} + opt_pos.long_name +
        '\n');

    auto optional_result = parse_optionals<T>(ret, argc, argp, argv);

    if (!optional_result.has_value()) {
      return std::unexpected(optional_result.error());
    }

    if (argp < argc) { // Positional argument exists
      std::optional<typename[:opt_pos.type:]> val =
          parse_arg<typename[:opt_pos.type:]>(argv[argp]);
      if (val.has_value()) {
        argp++;
        ret.[:opt_pos.val:] = *val;
      } else {
        return std::unexpected(err_parsing_string);
      }
    } else {
      break;
    }
  }

  if (argp < argc) { // More optionals exist
    auto optional_result = parse_optionals<T>(ret, argc, argp, argv);
    if (!optional_result.has_value()) {
      return std::unexpected(optional_result.error());
    }
  }

  if (argp < argc) { // We encountered an unexpected positional argument
    err_return_msg = "Error: Unknown argument: ";
    err_return_msg += argv[argp];
    err_return_msg += '\n';
    return std::unexpected(err_return_msg.c_str());
  }

  return ret;
}

} // namespace claire

#endif // CLAIRE_HPP
