#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include <charconv>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <meta>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>
#include <format>

namespace claire {

/// Extract the 'text' field from an annotated struct
template <std::meta::info i, typename T>
consteval const char *extract_text_annotation() {
  constexpr static auto shortnames =
      std::define_static_array(std::meta::annotations_of_with_type(i, ^^const T));

  template for (constexpr auto name : shortnames) {
    constexpr const char *txt = std::meta::extract<const T>(name).text;
    if (txt) {
      return txt;
    }
  }

  return std::define_static_string("");
}

/*
+----------------------------------------------------------------------------+
|                                                                            |
|                                  Structs                                   |
|                                                                            |
+----------------------------------------------------------------------------+
*/

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
  consteval Description(const std::string_view txt) : text(std::define_static_string(txt)) {}
  template <std::meta::info i>
  consteval static const char *extract() {
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
  consteval Shortname(const std::string_view txt) : text(std::define_static_string(txt)) {}
  template <std::meta::info i>
  consteval static const char *extract() {
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

/*
+----------------------------------------------------------------------------+
|                                                                            |
|                              Helper functions                              |
|                                                                            |
+----------------------------------------------------------------------------+
*/

/// Checks if a std::meta::info represents a type that is the same as T
template <std::meta::info i, typename T>
consteval bool same_type_as() {
  if (!std::meta::is_type(i)) {
    return std::meta::type_of(i) == ^^T;
  }
  return i == ^^T;
}

/// Checks if a C string is empty
constexpr inline bool not_emptystring(const char *s) { return s && s[0] != '\0'; }

/// Optional arguments are either boolean and assumed to be a flag
/// or wrapped in a std::optional
template <std::meta::info type>
consteval bool is_optional() {
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
constexpr auto get_fields() {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members =
      std::define_static_array(std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    constexpr std::meta::info member_type = std::meta::type_of(member);
    const char *member_name = std::define_static_string(std::meta::identifier_of(member));
    const char *member_desc = Description::extract<member>();
    const char *member_short_name = Shortname::extract<member>();
    bool opt = is_optional<member_type>();
    ArgumentDeets deets{member_name, member_short_name, member_desc, member_type, member, opt};
    fields.push_back(deets);
  }

  return std::define_static_array(fields);
}

/// Filters the fields to only return the positionals
/// #TODO This is inefficient
template <typename T>
constexpr auto get_positional_fields() {
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
constexpr auto get_optional_fields() {
  constexpr static auto fields = get_fields<T>();
  std::vector<ArgumentDeets> val;

  for (auto field : fields) {
    if (field.optional) {
      val.push_back(field);
    }
  }

  return std::define_static_array(val);
}

/*
+----------------------------------------------------------------------------+
|                                                                            |
|                               Value parsers                                |
|                                                                            |
+----------------------------------------------------------------------------+
*/

/// Parses a numeric value into from a c string
template <typename T>
inline std::optional<T> parse_numeric(const char *str) {
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
std::optional<T> parse_arg(const char *str) {
  return parse_numeric<T>(str);
}

/// Generic form of parse_arg for types that can be constructed from strings
template <typename T>
std::optional<T> parse_arg(const char *str) {
  try {
    return T{str};
  } catch (...) {
    return std::nullopt;
  }
}

/// If a argument is a boolean type, if it exists at all it is true
template <>
std::optional<bool> parse_arg<bool>([[maybe_unused]] const char *str) {
  return true;
}

template <typename T, ArgumentDeets deets>
inline bool parse_optional_wrapper(T &ret, int const argc, int &argp, const char **&argv) {
  constexpr const char *long_name = deets.long_name;
  if constexpr (same_type_as<deets.type, bool>()) {
    ret.[:deets.val:] = true;
    std::cout << "Found flag: " << long_name << '\n';
    return true;
  } else {
    if ((argp + 1) < argc) {
      ++argp; // #TODO: add in = handling to args. i.e. --file=filename
      auto result = parse_arg<typename[:deets.type:]>(argv[argp]);
      std::cout << "Found optional parameter: " << long_name << " value: " << argv[argp] << '\n';
      if (result) {
        ret.[:deets.val:] = result.value();
      }
      return true;
    } else {
      return false; // #TODO: proper error handling
    }
  }
}

template <typename T, ArgumentDeets deets, std::size_t offset, const char* name>
inline bool parse_optional_wrapper2(T &ret, int const argc, int &argp, const char **&argv) {
  if (strcmp(name, argv[argp] + offset) == 0) {
    return parse_optional_wrapper<T, deets>(ret, argc, argp, argv); // #TODO handle errors
  } else {
    return false;
  }
}

template <typename T, ArgumentDeets deets>
inline bool parse_short_optional(T &ret, int const argc, int &argp, const char **&argv) {
  if constexpr (not_emptystring(deets.short_name)) {
    return parse_optional_wrapper2<T, deets, 1, deets.short_name>(ret, argc, argp, argv);
  } else {
    return false;
  }
}

template <typename T, ArgumentDeets deets>
inline bool parse_long_optional(T &ret, int const argc, int &argp, const char **&argv) {
    return parse_optional_wrapper2<T, deets, 2, deets.long_name>(ret, argc, argp, argv);
}

/*
+----------------------------------------------------------------------------+
|                                                                            |
|                               Main functions                               |
|                                                                            |
+----------------------------------------------------------------------------+
*/

template <typename T>
consteval const char *create_help_string() {
  std::string s;

  constexpr auto program_desc = Description::extract<^^T>();

  if (not_emptystring(program_desc)) {
    s += program_desc;
    s += "\n\n";
  }

  constexpr auto static fields = get_fields<T>();
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

  template for (constexpr auto field : optionals) {
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

  char num[4096]{};

  if (fields.size() > 0 && fields.size() < 90) {
    s += "Fields: ";
    std::to_chars(num, num + 4095, fields.size());
    s += num;
  } else {
    s += "We have no fields?";
  }

  s += '\n';

  template for (constexpr auto field : fields) {
    std::to_chars(num, num + 4095, (int)field.optional);
    s += num + ' ';
    s += field.long_name;
    s += ' ';
    s += field.short_name;
    s += ' ';
    s += field.description;
    s += ' ';
    s += std::string{std::meta::display_string_of(field.type)};
    s += '\n';
  }

  return std::define_static_string(s);
}

template <typename T>
std::optional<T> parse_args(int argc, const char **argv) {
  constexpr static auto positionals = get_positional_fields<T>();
  constexpr static auto optionals = get_optional_fields<T>();
  T ret{};
  int argp = 1;

  // Iterate through the positional fields of the struct
  // Iterate through the argugments, if we find a argument that isn't a flag
  // i.e. `--verbose`, or a optional i.e. `--logging-level verbose`, break,
  // process the positional, and continue. #TODO: currently a flag doesn't
  // process correctly
  template for (constexpr auto field : positionals) {
    constexpr const char *err_parsing_string =
        std::define_static_string(std::string{"Failed parsing argument "} + field.long_name);
    constexpr const char *err_not_exists_string =
        std::define_static_string(std::string{"Failed parsing argument "} + field.long_name);

    constexpr const char *ln = field.long_name;
    constexpr bool opt = field.optional;

    std::cout << "Positional: " << ln << " optional: " << opt << '\n';

    // Iterate through remaining arguments
    for (; argp < argc; argp++) {
      const char *arg = argv[argp];
      std::cout << "Processing arg '" << arg << "'\n";

      std::cout << "argp: " << argp << " argc: " << argc << '\n';

      // Is it a optional argument?
      if (argv && arg[0] == '-') {

        // Short optional arugment
        if (arg[1] != '\0' && arg[1] != '-') {

          template for (constexpr auto option : optionals) {
            parse_short_optional<T, option>(ret, argc, argp, argv);
          }
        } else if (arg[1] == '-' && arg[2] != '\0') { // Long flag
          template for (constexpr auto option : optionals) {
            parse_long_optional<T, option>(ret, argc, argp, argv);
          }
        }
      } else {
        break;
      }
    }

    if (argp < argc) { // Positional argument exists
      std::optional<typename[:field.type:]> val = parse_arg<typename[:field.type:]>(argv[argp]);
      if (val.has_value()) {
        argp++;
        ret.[:field.val:] = *val;
      } else {
        std::puts(err_parsing_string);
        return std::nullopt;
      }
    } else {
      std::puts(err_not_exists_string);
      return std::nullopt;
    }
  }

  return std::optional<T>{ret};
}

} // namespace claire

#endif // CLAIRE_HPP
