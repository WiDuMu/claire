#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include "claire_parse_arg.hpp"
// #include "claire_annotation_structs.hpp"

#include <cstddef>
#include <meta>
#include <optional>

namespace claire {

struct Description {
  const char *text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  consteval Description(const std::string_view txt)
      : text(std::define_static_string(txt)) {}
};

struct Shortname {
  const char *text;
  // For some reason string literals are not actually static lifetime so the
  // compiler will freak out if you don't define it
  consteval Shortname(const std::string_view txt)
      : text(std::define_static_string(txt)) {}
};

template <std::meta::info i> constexpr const char *extract_description() {
  constexpr static auto descriptions = std::define_static_array(
      std::meta::annotations_of_with_type(i, ^^const Description));

  template for (constexpr auto desc : descriptions) {
    constexpr const char *txt =
        std::meta::extract<const Description>(desc).text;
    if (txt) {
      return txt;
    }
  }

  return std::define_static_string("");
}

template <std::meta::info i> constexpr const char *extract_shortname() {
  constexpr static auto shortnames = std::define_static_array(
      std::meta::annotations_of_with_type(i, ^^const Shortname));

  template for (constexpr auto name : shortnames) {
    constexpr const char *txt = std::meta::extract<const Shortname>(name).text;
    if (txt) {
      return txt;
    }
  }

  return std::define_static_string("");
}

constexpr inline bool not_emptystring(const char *s) {
  return s && s[0] != '\0';
}

struct ArgumentDeets {
  const char *long_name;
  const char *short_name;
  const char *description;
  std::meta::info type;
  std::meta::info val;
  bool optional;
};

template <std::meta::info type> consteval bool is_optional() {
  if (!std::meta::is_type(type) || !std::meta::has_template_arguments(type)) {
    return false;
  }

  return std::meta::template_of(type) == ^^std::optional;
}

template <typename T> constexpr auto get_fields() {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    std::meta::info member_type = std::meta::type_of(member);
    const char *member_name =
        std::define_static_string(std::meta::identifier_of(member));
    const char *member_desc = extract_description<member>();
    const char *member_short_name = extract_shortname<member>();
    bool opt = is_optional<member>();
    ArgumentDeets deets{
        member_name, member_short_name, member_desc, member_type, member, opt};
    fields.push_back(deets);
  }

  return std::define_static_array(fields);
}

template <typename T> constexpr auto get_positional_fields() {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    constexpr std::meta::info member_type = std::meta::type_of(member);
    bool opt = is_optional<member_type>();
    if (!opt) {
      const char *member_name =
          std::define_static_string(std::meta::identifier_of(member));
      const char *member_desc = extract_description<member>();
      const char *member_short_name = extract_shortname<member>();
      bool opt = is_optional<member_type>();
      ArgumentDeets deets{member_name, member_short_name,
                          member_desc, member_type,
                          member,      opt};
      fields.push_back(deets);
    }
  }

  return std::define_static_array(fields);
}

template <typename T> constexpr auto get_optional_fields() {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members = std::define_static_array(
      std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    constexpr std::meta::info member_type = std::meta::type_of(member);
    bool opt = is_optional<member_type>();
    if (!opt) {
      const char *member_name =
          std::define_static_string(std::meta::identifier_of(member));
      const char *member_desc = extract_description<member>();
      const char *member_short_name = extract_shortname<member>();
      bool opt = is_optional<member_type>();
      ArgumentDeets deets{member_name, member_short_name,
                          member_desc, member_type,
                          member,      opt};
      fields.push_back(deets);
    }
  }

  return std::define_static_array(fields);
}

template <typename T> consteval auto create_help_string() {
  std::string s;

  constexpr auto program_desc = extract_description<^^T>();

  if (not_emptystring(program_desc)) {
    s += program_desc;
    s += "\n\n";
  }

  constexpr auto static fields = get_fields<T>();

  if (fields.size()) {
    s += "USAGE:\n";
  }

  template for (constexpr auto field : fields) {
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

  return s;
}

template <typename T> consteval const char *const struct_fields() {
  return define_static_string(create_help_string<T>());
}

template <typename T> std::optional<T> parse_args(int argc, const char **argv) {
  constexpr static auto deets = get_positional_fields<T>();
  T ret{};
  std::size_t argp = 1;
  std::size_t positional_index = 0;

  template for (constexpr auto field : deets) {

    for (argp; argp < argc; argp++) {
      const char *arg = argv[argp];
      // Are we a flag?
      if (argv && arg[0] == '-') {
        if (arg[1] != '\0' && arg[1] != '-') {        // Short flag
        } else if (arg[1] == '-' && arg[2] != '\0') { // Long flag
        }
      } else {
        break;
      }
    }

    constexpr const char *err_parsing_string = std::define_static_string(
        std::string{"Failed parsing argument "} + field.long_name);
    constexpr const char *err_not_exists_string = std::define_static_string(
        std::string{"Failed parsing argument "} + field.long_name);

    if (argp < argc) { // Positional argument exists
      std::optional<typename[:field.type:]> val =
          parse_arg<typename[:field.type:]>(argv[argp]);
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
