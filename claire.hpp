#ifndef CLAIRE_HPP
#define CLAIRE_HPP

#include <meta>

namespace claire {

struct Description {
  const char *text;
  // For some reason string literals are not actually static lifetime so the compiler will freak out if you don't define it
  consteval Description(const std::string_view txt) : text(std::define_static_string(txt)) {}
};

struct Shortname {
  const char *text;
  // For some reason string literals are not actually static lifetime so the compiler will freak out if you don't define it
  consteval Shortname(const std::string_view txt) : text(std::define_static_string(txt)) {}
};

template <std::meta::info i>
constexpr auto extract_description() {
  constexpr static auto descriptions = std::define_static_array(std::meta::annotations_of_with_type(i, ^^const Description));

  if (descriptions.size() == 0) {
    return "No description";
  }

  template for (constexpr auto desc : descriptions) {
    constexpr const char* txt = std::meta::extract<const Description>(desc).text;
    if (txt) {
      return txt;
    }
  }
}

template <std::meta::info i>
constexpr auto extract_shortname() {
  constexpr static auto shortnames = std::define_static_array(std::meta::annotations_of_with_type(i, ^^const Shortname));

  if (shortnames.size() == 0) {
    return nullptr;
  }

  template for (constexpr auto name : shortnames) {
    constexpr const char* txt = std::meta::extract<const Shortname>(name).text;
    if (txt) {
      return txt;
    }
  }
}

struct ArgumentDeets {
  const char * long_name;
  const char * short_name;
  const char * description;
  std::meta::info type;
};

template <typename T>
constexpr auto get_fields() {
  std::vector<ArgumentDeets> fields{};

  constexpr auto context = std::meta::access_context::current();
  constexpr auto static members =
      std::define_static_array(std::meta::nonstatic_data_members_of(^^T, context));

  template for (constexpr auto member : members) {
    std::meta::info member_type = std::meta::type_of(member);
    const char * member_name = std::meta::identifier_of(member);
    const char * member_desc = extract_description<member>();
    const char * member_short_name = extract_shortname<member>();
    ArgumentDeets deets{member_name, member_short_name, member_desc, member_type};
    fields.push_back(deets);
  }


  return std::define_static_array(fields);
}

template <typename T> constexpr auto struct_fields_h() {
  std::string s;

  s += std::meta::identifier_of(^^T);
  s += "\n";

  constexpr auto ctx = std::meta::access_context::unchecked();
  constexpr auto static members =
      std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));

  template for (constexpr auto member : members) {

    constexpr std::meta::info type_info = std::meta::type_of(member);

    auto type_name = std::meta::display_string_of(type_info);
    auto field_name = std::meta::identifier_of(member);

    s += "  - ";
    s += type_name;
    s += " ";
    s += field_name;
    s += "\n";
    s += extract_description<member>();
    s += '\n';
  }

  return s;
}

template <typename T> consteval const char *const struct_fields() {
  return define_static_string(struct_fields_h<T>());
}

} // namespace claire

#endif // CLAIRE_HPP
