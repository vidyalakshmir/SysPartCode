# Makefile
CXX = g++
CXXFLAGS = -I. -I./hello -fPIC -g
LDFLAGS = -L./hello -lhello -Wl,-rpath=./hello

all: hello/libhello.so hello/main helloIndirect/main helloDynamic/main

hello/libhello.so: hello/hello.cpp
	$(CXX) $(CXXFLAGS) -shared hello/hello.cpp -o hello/libhello.so

hello/main: hello/main.cpp hello/libhello.so
	$(CXX) $(CXXFLAGS) hello/main.cpp -o hello/main $(LDFLAGS)

helloIndirect/main: helloIndirect/main.cpp hello/libhello.so
	$(CXX) $(CXXFLAGS) helloIndirect/main.cpp -o helloIndirect/main $(LDFLAGS)

helloDynamic/main: helloDynamic/main.cpp
	$(CXX) $(CXXFLAGS) helloDynamic/main.cpp -o helloDynamic/main -ldl

clean:
	rm -f hello/*.o hello/*.so hello/main helloIndirect/main helloDynamic/main

