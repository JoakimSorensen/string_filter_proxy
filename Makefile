CFLAGS= -Wall -pedantic
CXX           = g++

all: string_proxy

####### Compile
string_proxy:
	$(CXX) $(CFLAGS)  src/main.cpp src/serverside.h src/serverside.cpp src/clientside.h src/clientside.cpp -o build/string_proxy
