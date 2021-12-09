CXX =		g++
CXXFLAGS =	-std=c++14 -march=native -g -O3 -I./

all:	hot-example

hot-example:	hot-example.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf hot-example

