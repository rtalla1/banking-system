CXX = g++
CXXFLAGS = -std=c++11 -Wall -g -pthread
LDFLAGS = -pthread

# Common objects
COMMON_OBJS = common.o signals.o thread_pool.o network_channel.o

# Server executables
SERVERS = finance file logging

# Client executable
CLIENT = client

# All targets
all: $(SERVERS) $(CLIENT)

# Object rules
%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

common.o: common.cpp common.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

signals.o: signals.cpp signals.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

thread_pool.o: thread_pool.cpp thread_pool.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

network_channel.o: network_channel.cpp network_channel.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Server executables
finance: finance.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

file: file.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

logging: logging.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Client executable
client: client.o $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Source dependencies
finance.o: finance.cpp common.h network_channel.h thread_pool.h signals.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

file.o: file.cpp common.h network_channel.h thread_pool.h signals.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

logging.o: logging.cpp common.h network_channel.h thread_pool.h signals.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

client.o: client.cpp common.h network_channel.h signals.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f *.o $(SERVERS) $(CLIENT)
	rm -rf storage
	rm -f *.log
	rm -rf test_output
	rm -rf test_files
	rm -rf test_storage
	rm -rf test_results
	rm -f *.txt
	rm -rf build
	rm -rf test_dir
	rm -rf test_*

.PHONY: all clean