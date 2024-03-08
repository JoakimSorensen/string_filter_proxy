CXXFLAGS= -Wall -pedantic
CXX           = g++

all: string_proxy

####### Compile
string_proxy:
	$(CXX) $(CXXFLAGS)  build/main.o build/serverside.o build/clientside.o -o build/string_proxy

main:
	$(CXX) $(CXXFLAGS) -c src/main.cpp -o build/main.o

serverside:
	$(CXX) $(CXXFLAGS) -c src/serverside.cpp -o build/serverside.o
clientside:
	$(CXX) $(CXXFLAGS) -c src/clientside.cpp -o build/clientside.o
