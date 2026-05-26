CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -Iinclude
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2

TARGET = embedded_system_demo
TEST_TARGET = run_tests

SRCS = src/main.cpp \
       src/uart_driver/simulated_uart.cpp \
       src/scheduler/simple_scheduler.cpp \
       src/socket_protocol/socket_protocol.cpp

TEST_SRCS = tests/test_all.cpp \
            src/uart_driver/simulated_uart.cpp \
            src/scheduler/simple_scheduler.cpp

OBJS = $(SRCS:.cpp=.o)
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

.PHONY: all clean debug release test

all: release

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	./$(TEST_TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

# Platform specific settings
ifeq ($(OS), Windows_NT)
    CXXFLAGS += -lws2_32
endif