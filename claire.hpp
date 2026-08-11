/*-----------------------------------------------------------------------------+
|                                                                              |
|       Claire: Command Line Arguments Interpretation Reflection Engine        |
|                             (C) WiDuMu 2026                                  |
|                                                                              |
+-----------------------------------------------------------------------------*/

#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include <charconv>
#include <cstddef>
#include <cstring>
#include <expected>
#include <meta>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace claire {

/// Extract the 'text' field from an annotated struct
template <std::meta::info i, typename T>
[[nodiscard]] consteval const char *extract_text_annotation() noexcept {
  constexpr static auto shortnames = std::define_static_array(
      std::meta::annotations_of_with_type(i, ^^const T));

  template for (constexpr auto name : shortnames) {
    constexpr const char *txt = std::meta::extract<const T>(name).text;
    if (txt) {
      return txt;
    }
  }

  return std::define_static_string("");
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                                  Structs                                   |
|                                                                            |
+---------------------------------------------------------------------------*/

/// A description of a parameter
/// Usage:
/// struct Args {
///    [[= Description("The username to use to log in")]]
///    const char* user_name;
/// }
struct Description {
  const char *text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  [[nodiscard]] consteval Description(const std::string_view txt) noexcept
      : text(std::define_static_string(txt)) {}
  template <std::meta::info i>
  [[nodiscard]] consteval static const char *extract() noexcept {
    return extract_text_annotation<i, Description>();
  }
};

/// A optional short name for a parameter
/// Usage:
/// struct Args {
///    [[= Shortname("U")]]
///    const char* user_name;
/// }
struct Shortname {
  const char *text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  [[nodiscard]] consteval Shortname(const std::string_view txt) noexcept
      : text(std::define_static_string(txt)) {}
  template <std::meta::info i>
  [[nodiscard]] consteval static const char *extract() noexcept {
    return extract_text_annotation<i, Shortname>();
  }
};

/// Internal structure used to store details of each argument provided
struct ArgumentDeets {
  const char *long_name;
  const char *short_name;
  const char *description;
  std::meta::info type;
  std::meta::info val;
  bool optional;
};

/*---------------------------------------------------------------------------+
|                                                                            |
|                           Library statics                                  |
|                                                                            |
+---------------------------------------------------------------------------*/

inline std::string err_return_msg;

/*---------------------------------------------------------------------------+
|                                                                            |
|                              Helper functions                              |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Checks if a std::meta::info represents a type that is the same as T
template <std::meta::info i, typename T>
[[nodiscard]] consteval bool same_type_as() noexcept {
  if (!std::meta::is_type(i)) {
    return std::meta::type_of(i) == ^^T;
  }
  return i == ^^T;
}

/// Checks if a C string is empty
constexpr inline bool not_emptystring(const char *s) {
  return s && s[0] != '\0';
}

/// Optional arguments are either boolean and assumed to be a flag
/// or wrapped in a std::optional
template <std::meta::info type>
[[nodiscard]] consteval bool is_optional() noexcept {
  if (!std::meta::is_type(type)) {
    return false;
  }

  if (type == ^^bool) {
    return true;
  }

  if (!std::meta::has_template_arguments(type)) {
    return false;
  }

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
    const char *member_name =
        std::define_static_string(std::meta::identifier_of(member));
    const char *member_desc = Description::extract<member>();
    const char *member_short_name = Shortname::extract<member>();
    bool opt = is_optional<member_type>();
    ArgumentDeets deets{
        member_name, member_short_name, member_desc, member_type, member, opt};
    fields.push_back(deets);
  }

  return std::define_static_array(fields);
}

/// Filters the fields to only return the positionals
/// #TODO This is inefficient
template <typename T>
[[nodiscard]] constexpr auto get_positional_fields() noexcept {
  constexpr static auto fields = get_fields<T>();
  std::vector<ArgumentDeets> val;

  for (auto field : fields) {
    if (!field.optional) {
      val.push_back(field);
    }
  }

  return std::define_static_array(val);
}

/// Filters the fields to only return the optional values
/// #TODO this is inefficient
template <typename T>
[[nodiscard]] constexpr auto get_optional_fields() noexcept {
  constexpr static auto fields = get_fields<T>();
  std::vector<ArgumentDeets> val;

  for (auto field : fields) {
    if (field.optional) {
      val.push_back(field);
    }
  }

  return std::define_static_array(val);
}

/*---------------------------------------------------------------------------+
|                                                                            |
|                               Value parsers                                |
|                                                                            |
+---------------------------------------------------------------------------*/

/// Parses a numeric value into from a c string
template <typename T>
[[nodiscard]] inline std::optional<T> parse_numeric(const char *str) noexcept {
  size_t len = std::strlen(str);
  T val;
  auto result = std::from_chars(str, str + len, val);
  if (result) {
    return val;
  }
  return std::nullopt;
}

/// Specialization of generic function parse_arg for numeric types
/// Uses parse_numeric
template <typename T>
  requires std::integral<T> || std::floating_point<T>
[[nodiscard]] std::optional<T> parse_arg(const char *str) noexcept {
  return parse_numeric<T>(str);
}

/// Generic form of parse_arg for types that can be constructed from strings
template <typename T>
[[nodiscard]] std::optional<T> parse_arg(const char *str) noexcept {
  try {
    return T{str};
  } catch (...) {
    return std::nullopt;
  }
}

/// If a argument is a boolean type, if it exists at all it is true
template <>
[[nodiscard]] std::optional<bool>
parse_arg<bool>([[maybe_unused]] const char *str) noexcept {
  return true;
}

template <typename T, ArgumentDeets deets, std::size_t offset, const char *name>
[[nodiscard]] inline std::expected<bool, const char *>
parse_optional(T &ret, int const argc, int &argp, const char **&argv) noexcept {
  constexpr const char *const err_parsing_msg = std::define_static_string(
      std::string{"Error: failed to parse argument '"} + name + "'\n");
  constexpr const char *const err_missing_msg = std::define_static_string(
      std::string{"Error: missing value for argument '"} + name + "'\n");

  if (strcmp(name, argv[argp] + offset)) { // If we don't match bail
    return false;
  }

  if constexpr (same_type_as<deets.type, bool>()) {
    ret.[:deets.val:] = true;
    return true;
  } else {

    if ((argp + 1) >= argc) {
      return std::unexpected(err_missing_msg);
    }

    ++argp; // #TODO: add in = handling to args. i.e. --file=filename
    auto result = parse_arg<typename[:deets.type:]>(argv[argp]);
    if (result) {
      ret.[:deets.val:] = result.value();
      return true;
    } else {
      return std::unexpected(err_parsing_msg);
    }
  }
}

template <typename T>
[[nodiscard]] inline std::expected<bool, const char *>
parse_optionals(T &ret, int const argc, int &argp,
                const char **&argv) noexcept {
  constexpr static auto optionals = get_optional_fields<T>();
  for (; argp < argc; argp++) {
    const char *arg = argv[argp];

    // Is it a optional argument?
    if (argv && arg[0] == '-') {

      // Short optional arugment
      if (arg[1] != '\0' && arg[1] != '-') {

        template for (constexpr auto option : optionals) {

          if constexpr (not_emptystring(option.short_name)) {
            auto result = parse_optional<T, option, 1, option.short_name>(
                ret, argc, argp, argv);
            if (!result.has_value()) {
              return std::unexpected(result.error());
            }
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
          if (!result.has_value()) {
            return std::unexpected(result.error());
          }
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
consteval const char *create_help_string() {
  std::string s;

  constexpr auto program_desc = Description::extract<^^T>();

  if (not_emptystring(program_desc)) {
    s += program_desc;
    s += "\n\n";
  }

  constexpr auto static positionals = get_positional_fields<T>();
  constexpr auto static optionals = get_optional_fields<T>();

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

  if (optionals.size()) {
    s += "Options:\n";
  }

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
std::expected<T, const char *> parse_args(int argc, const char **argv) {
  constexpr static auto positionals = get_positional_fields<T>();
  T ret{};
  int argp = 1;

  // Iterate through the positional fields of the struct
  // Iterate through the argugments, if we find a argument that isn't a flag
  // i.e. `--verbose`, or a optional i.e. `--logging-level verbose`, break,
  // process the positional, and continue. #TODO: currently a flag doesn't
  // process correctly
  template for (constexpr auto field : positionals) {
    constexpr const char *const err_parsing_string = std::define_static_string(
        std::string{"Error: Failed parsing argument "} + field.long_name +
        '\n');
    constexpr const char *const err_not_exists_string =
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
