cmake_minimum_required(VERSION 3.11)
project(Claire)

enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wpedantic -Wno-unused-parameter -freflection")
set(CLAIRE_INCLUDES src/include)

# set(CMAKE_CXX_FLAGS "")

add_executable(claire_simple_test src/main.cpp)
target_include_directories(claire_simple_test PRIVATE ${CLAIRE_INCLUDES})

# GoogleTest Stuff
include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


add_executable(
  claire_unit_tests
  src/tests/tests.cpp
)
target_include_directories(claire_unit_tests PRIVATE ${CLAIRE_INCLUDES})
target_link_libraries(
  claire_unit_tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(claire_unit_tests)

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    # COMMENT "Running test suite..."
)
add_dependencies(run_all_tests claire_unit_tests)

# gtest_discover_tests(claire_unit_tests)
