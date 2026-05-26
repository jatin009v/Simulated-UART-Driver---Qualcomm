#include "../include/common.hpp"
#include "../src/uart_driver/simulated_uart.hpp"
#include "../src/scheduler/simple_scheduler.hpp"
#include <cassert>
#include <iostream>

void testUARTDriver() {
    Logger::log(LogLevel::INFO, "TEST", "Testing UART Driver...");
    
    SimulatedUART uart("TEST_UART");
    
    // Test initialization
    SimulatedUART::UARTConfig config;
    assert(uart.init(config));
    assert(uart.isInitialized());
    
    // Test double initialization
    assert(!uart.init(config));
    
    // Test data transmission
    assert(uart.sendByte('A'));
    assert(uart.sendString("Hello"));
    
    // Test data reception
    std::vector<uint8_t> testData = {'W', 'o', 'r', 'l', 'd'};
    uart.simulateDataReception(testData);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(uart.availableData() > 0);
    
    // Test statistics
    auto stats = uart.getStatistics();
    assert(stats.bytesSent > 0);
    assert(stats.bytesReceived > 0);
    
    Logger::log(LogLevel::INFO, "TEST", "UART Driver tests passed!");
}

void testScheduler() {
    Logger::log(LogLevel::INFO, "TEST", "Testing Scheduler...");
    
    SimpleScheduler scheduler("TestScheduler");
    
    int counter = 0;
    
    // Test task addition
    assert(scheduler.addTask("TestTask1", [&counter]() { counter++; }));
    assert(scheduler.addTask("TestTask2", [&counter]() { counter += 2; }, 
                            SimpleScheduler::TaskPriority::HIGH));
    
    assert(scheduler.getTotalTasks() == 2);
    
    // Test task execution
    scheduler.runOnce();
    scheduler.runOnce();
    
    assert(counter > 0);
    
    // Test task suspension
    assert(scheduler.suspendTask("TestTask1"));
    
    // Test task removal
    assert(scheduler.removeTask("TestTask2"));
    assert(scheduler.getTotalTasks() == 1);
    
    Logger::log(LogLevel::INFO, "TEST", "Scheduler tests passed!");
}

int main() {
    try {
        testUARTDriver();
        testScheduler();
        
        Logger::log(LogLevel::INFO, "TEST", "All tests passed successfully!");
        return 0;
    } catch (const std::exception& e) {
        Logger::log(LogLevel::ERROR, "TEST", std::string("Test failed: ") + e.what());
        return 1;
    }
}