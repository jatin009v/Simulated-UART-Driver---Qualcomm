# Embedded Systems Components in C++

This project implements three essential components commonly used in embedded systems and IoT applications using modern C++:

1. **Simulated UART Driver**
2. **Simple Task Scheduler**
3. **Socket-Based Communication Protocol**

## System Architecture

### Component Overview

┌─────────────────────────────────────────────────────────────┐
│ Application Layer │
├─────────────────────────────────────────────────────────────┤
│ ┌──────────────┐ ┌──────────────┐ ┌──────────────────┐ │
│ │ UART Driver │ │ Scheduler │ │ Socket Protocol │ │
│ │ │ │ │ │ │ │
│ │ • TX/RX │ │ • Task Mgmt │ │ • Frame Protocol │ │
│ │ • Interrupts │ │ • Priority │ │ • Server/Client │ │
│ │ • DMA │ │ • Periodic │ │ • CRC Validation │ │
│ │ • Callbacks │ │ • Statistics │ │ • Heartbeat │ │
│ └──────────────┘ └──────────────┘ └──────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ Common Utilities │
│ (Logger, Types, Constants) │
└─────────────────────────────────────────────────────────────┘

## Features

### 1. Simulated UART Driver

The UART driver provides a comprehensive simulation of hardware UART communication with support for:

- **Multiple Operating Modes**: Polling, Interrupt, and DMA
- **Configurable Parameters**: Baud rate, data bits, parity, stop bits
- **Buffered Communication**: TX and RX buffers with overflow protection
- **Callback System**: Event-driven architecture for received data
- **Error Simulation**: Parity errors, overrun, and framing errors
- **Statistics Tracking**: Bytes sent/received, error counts

#### Design Flow:
Application → sendByte() → TX Buffer → Worker Thread → Hardware Simulation
Hardware Sim → RX Buffer → Callback/readByte() → Application


### 2. Simple Task Scheduler

A priority-based cooperative scheduler designed for embedded systems:

- **Task Management**: Add, remove, suspend, resume tasks
- **Priority Levels**: LOW, NORMAL, HIGH, CRITICAL
- **Periodic Execution**: Tasks can be scheduled at fixed intervals
- **Task Statistics**: Execution count, timing information
- **Thread-Safe**: Mutex-protected task list

#### Scheduling Algorithm:
Select highest priority READY task

Check if periodic task is due

Execute task function

Update statistics and next run time

Repeat with scheduling interval


### 3. Socket-Based Protocol

A custom communication protocol over TCP sockets:

- **Frame-Based Protocol**: Start/end markers, length field, CRC
- **Message Types**: DATA, ACK, NACK, HEARTBEAT, COMMAND, RESPONSE
- **State Machine Receiver**: Robust byte-by-byte frame assembly
- **Server/Client Architecture**: Support for both modes
- **Automatic ACK**: Optional acknowledgment system
- **Error Detection**: CRC-16 validation

#### Protocol Frame Format:
┌──────┬────────┬──────┬──────────┬─────────┬──────┬──────┐
│START │ LENGTH │ TYPE │ SEQUENCE │ PAYLOAD │ CRC │ END │
│0xAA │ 2 bytes│1 byte│ 1 byte │ N bytes │2 bytes│0x55 │
└──────┴────────┴──────┴──────────┴─────────┴──────┴──────┘


## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- POSIX-compatible OS (Linux, macOS) for socket implementation
- Make build system

### Building

```bash
# Build release version
make

# Build debug version
make debug

# Run tests
make test

# Run the demo
make run

# Clean build files
make clean

Usage Examples
UART Driver
SimulatedUART uart("UART1");
SimulatedUART::UARTConfig config;
config.baudRate = SimulatedUART::BaudRate::B115200;

uart.init(config);
uart.sendString("Hello World!");

// Simulate receiving data
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
uart.simulateDataReception(data);

Task Scheduler
cpp
SimpleScheduler scheduler;

scheduler.addTask("LEDTask", []() {
    // Toggle LED
}, SimpleScheduler::TaskPriority::NORMAL, 
   std::chrono::milliseconds(500));

scheduler.start();
// Scheduler runs in background


Socket Protocol
cpp
SocketProtocol server;
SocketProtocol::ProtocolConfig config;
config.port = 8080;

server.startServer(config);

// Create and send frame
auto frame = SocketProtocol::createFrame(
    SocketProtocol::FrameType::DATA,
    {'H', 'e', 'l', 'l', 'o'}
);

Threading Model

Main Thread: Application initialization and control
UART Worker Thread: Interrupt/DMA mode data processing
Scheduler Thread: Task execution and timing
Protocol Server Thread: Accepting new connections
Protocol Receive Thread: Frame reception per connection
Memory Management
Stack allocation for small, short-lived objects

Shared pointers for task management

Circular buffers for UART data

Pre-allocated buffers for protocol frames

Error Recovery
UART: Buffer overrun detection, parity error counting

Scheduler: Task exception isolation, automatic task removal on fatal errors

Protocol: Frame validation, CRC checking, connection timeout handling

Testing
The test suite covers:

Unit tests for each component

Integration tests for component interaction

Stress tests for buffer management

Error condition simulation

Performance Considerations
UART: Up to 115200 baud simulated communication

Scheduler: <1ms task switching overhead

Protocol: Efficient frame parsing with O(n) complexity

Extending the System
Adding New UART Features
Extend the SimulatedUART class

Add new configuration options

Implement hardware-specific simulations

Custom Task Types
Create task wrapper classes

Implement different scheduling algorithms

Add task dependency management

Protocol Extensions
Add new frame types

Implement encryption

Add compression support

License
This project is provided for educational purposes. Feel free to use and modify.

Contributing
Contributions are welcome! Please focus on:

Code quality and documentation

Test coverage

Performance improvements

Platform portability

Contact
For questions or suggestions, please create an issue in the repository.


