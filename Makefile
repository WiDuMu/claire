CXX = g++
CXX_FLAGS = --std=c++26 -freflection -flto -Wall -Wconversion -Wextra -Wshadow -Wpedantic -Werror
CXX_INCLUDES  = -Iinclude

all:
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) src/main.cpp -o hello_test
