# Claire

Claire is a header-only library that leverages C++26 reflection to generate a command line argument parser from a single struct:

```cpp
#include <iostream>

#include "claire.hpp"

using claire::Description, claire::Shortname;

struct 
[[= Description("Greeting generator")]]
Args {
   [[= Description("Name to greet")]]
   std::string name;
   [[= Description("Print this help string"), = Shortname("h")]]
   bool help;
   [[= Description("Print version"), = Shortname("V")]]
   bool version;
};

int main(int argc, const char** argv) {
   auto args_result = claire::parse_args<Args>(argc, argv);

   if (!args_result) {
      std::cerr << args_result.error();
      return 1;
   }

   auto args = *args_result;

   if (args.version) {
      std::cout << "Greeting 1.0\n";
      return 0;
   }

   if (args.help) {
      std::cout << claire::create_help_string<Args>();
      return 0;
   }

   std::cout << "Hello, " << args.name << "\n";
}
```

```
$ ./greet hello -h
Greeting generator

USAGE: <name>
   name Name to greet
Options:
   -h --help Print this help string
   -V --version Print version
```

# Todo
- [ ] Optional positionals
- [ ] Bypassing flags (flags which bypass positional requirements, i.e. --help)
- [ ] Unit Testing
- [ ] Subcommands
- [ ] Proper enum types
