VPATH=.
INCLUDE=$(shell pkg-config --cflags libconfig)
DEFINE=-D_BSD_SOURCE
WPROBED_CFLAGS=-Wall -std=c11 $(DEFINE) $(INCLUDE) -g 
WPROBED_CXXFLAGS=-Wall -std=c++11 $(DEFINE) $(INCLUDE) -g
WPROBED_LDFLAGS=-lpcap -lcurl -lsqlite3 -lconfig -lpthread $(shell pkg-config --libs libconfig) -g

all: wprobed

%.c.o: %.c
	$(CC) $(WPROBED_CFLAGS) $< -c -o $@

%.cpp.o: %.cpp
	$(CXX) $(WPROBED_CXXFLAGS) $< -c -o $@

wprobed: main.cpp.o service.cpp.o service_probing.cpp.o service_upstream.cpp.o MacTimestampMap.cpp.o MacEventUploader.cpp.o configuration.cpp.o
	$(CXX) $(WPROBED_LDFLAGS) $^ -o $@

clean:
	rm -f *.o
