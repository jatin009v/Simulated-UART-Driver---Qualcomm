#include "uart_driver/simulated_uart.hpp"
#include "scheduler/simple_scheduler.hpp"
#include "socket_protocol/socket_protocol.hpp"

// Example usage demonstrating all three components
int main() {
    Logger::log(LogLevel::INFO, "MAIN", "Starting Embedded Systems Demo");
    
    // 1. UART Driver Demo
    Logger::log(LogLevel::INFO, "MAIN", "=== UART Driver Demo ===");
    
    SimulatedUART uart("UART0");
    SimulatedUART::UARTConfig config;
    config.baudRate = SimulatedUART::BaudRate::B115200;
    config.mode = SimulatedUART::UARTMode::INTERRUPT;
    
    if (uart.init(config)) {
        // Register callback
        uart.registerRxCallback([](const std::vector<uint8_t>& data) {
            Logger::log(LogLevel::INFO, "UART_CB", 
                       "Received " + std::to_string(data.size()) + " bytes");
        });
        
        // Simulate data reception
        std::vector<uint8_t> testData = {'H', 'e', 'l', 'l', 'o'};
        uart.simulateDataReception(testData);
        
        // Read data
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (uart.availableData() > 0) {
            std::string received = uart.readString();
            Logger::log(LogLevel::INFO, "UART_DEMO", "Read: " + received);
        }
        
        // Send data
        uart.sendString("UART Test Message");
        
        auto stats = uart.getStatistics();
        Logger::log(LogLevel::INFO, "UART_DEMO", 
                   "Stats - Sent: " + std::to_string(stats.bytesSent) +
                   ", Received: " + std::to_string(stats.bytesReceived));
    }
    
    // 2. Scheduler Demo
    Logger::log(LogLevel::INFO, "MAIN", "\n=== Scheduler Demo ===");
    
    SimpleScheduler scheduler("DemoScheduler");
    
    // Add tasks
    scheduler.addTask("SensorRead", []() {
        Logger::log(LogLevel::INFO, "TASK", "Reading sensor data...");
    }, SimpleScheduler::TaskPriority::HIGH, std::chrono::milliseconds(100));
    
    scheduler.addTask("DataProcess", []() {
        Logger::log(LogLevel::INFO, "TASK", "Processing data...");
    }, SimpleScheduler::TaskPriority::NORMAL, std::chrono::milliseconds(200));
    
    scheduler.addTask("Heartbeat", []() {
        Logger::log(LogLevel::DEBUG, "TASK", "Heartbeat check");
    }, SimpleScheduler::TaskPriority::LOW, std::chrono::milliseconds(500));
    
    // Start scheduler
    scheduler.start();
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // Check task statistics
    auto taskList = scheduler.getTaskList();
    for (const auto& task : taskList) {
        Logger::log(LogLevel::INFO, "SCHEDULER_DEMO", 
                   "Task: " + task.name + 
                   " Executions: " + std::to_string(task.executionCount));
    }
    
    scheduler.stop();
    
    // 3. Socket Protocol Demo
    Logger::log(LogLevel::INFO, "MAIN", "\n=== Socket Protocol Demo ===");
    
    SocketProtocol server;
    SocketProtocol client;
    
    // Start server
    SocketProtocol::ProtocolConfig serverConfig;
    serverConfig.port = 8081;
    serverConfig.useAck = false;
    
    if (server.startServer(serverConfig)) {
        Logger::log(LogLevel::INFO, "MAIN", "Server started on port 8081");
        
        // Connect client in a separate thread
        std::thread clientThread([&client]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            if (client.connectToServer("127.0.0.1", 8081)) {
                Logger::log(LogLevel::INFO, "CLIENT", "Connected to server");
                
                // Create and send frame
                std::vector<uint8_t> payload = {'T', 'E', 'S', 'T'};
                auto frame = SocketProtocol::createFrame(
                    SocketProtocol::FrameType::DATA, payload);
                
                client.sendFrame(frame);
                Logger::log(LogLevel::INFO, "CLIENT", "Frame sent");
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                client.disconnect();
            }
        });
        
        clientThread.join();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        server.stopServer();
    }
    
    Logger::log(LogLevel::INFO, "MAIN", "\n=== Demo Complete ===");
    return 0;
}