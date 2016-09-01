VPATH=.:./sqlite3
DEFINE=-D_BSD_SOURCE
WPROBED_CFLAGS=-Wall -std=c11 $(DEFINE) -g
WPROBED_CXXFLAGS=-Wall -std=c++11 $(DEFINE) -g
WPROBED_LDFLAGS=-lpcap -lcurl -lsqlite3 -lconfig -lpthread -g

all: wprobed

%.c.o: %.c
	$(CC) $(WPROBED_CFLAGS) $< -c -o $@

%.cpp.o: %.cpp
	$(CXX) $(WPROBED_CXXFLAGS) $< -c -o $@

wprobed: main.cpp.o service.cpp.o service_probing.cpp.o MacTimestampMap.cpp.o
	$(CXX) $(WPROBED_LDFLAGS) $^ -o $@

clean:
	rm -f *.o
