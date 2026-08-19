CXX = g++
CXX_FLAGS = --std=c++26 -freflection -flto -Wall -Wconversion -Wextra -Wshadow -Wpedantic -Werror
CXX_INCLUDES  = -I.

all:
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) main.cpp -o hello_test

test: test_main.cpp claire.hpp
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) test_main.cpp -o test
	./test
