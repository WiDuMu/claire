CXX = g++
CXX_FLAGS = --std=c++26 -freflection -Wall -Wpedantic
CXX_INCLUDES  = -I.

all:
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) main.cpp -o hello_test