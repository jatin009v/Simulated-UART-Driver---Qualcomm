#include "simulated_uart.hpp"

SimulatedUART::SimulatedUART(const std::string& portName) 
    : portName_(portName), initialized_(false), running_(false) {
    Logger::log(LogLevel::DEBUG, "UART", "Created UART instance: " + portName);
}

SimulatedUART::~SimulatedUART() {
    deinit();
    Logger::log(LogLevel::DEBUG, "UART", "Destroyed UART instance: " + portName_);
}

bool SimulatedUART::init(const UARTConfig& config) {
    if (initialized_) {
        Logger::log(LogLevel::WARNING, "UART", "UART already initialized");
        return false;
    }
    
    config_ = config;
    initialized_ = true;
    
    Logger::log(LogLevel::INFO, "UART", 
        "Initializing " + portName_ + 
        " with baud=" + std::to_string(static_cast<int>(config.baudRate)) +
        " data=" + std::to_string(static_cast<int>(config.dataBits)) +
        " mode=" + (config.mode == UARTMode::INTERRUPT ? "INTERRUPT" : 
                   config.mode == UARTMode::DMA ? "DMA" : "POLLING"));
    
    if (config_.mode == UARTMode::INTERRUPT || config_.mode == UARTMode::DMA) {
        running_ = true;
        workerThread_ = std::make_unique<std::thread>(&SimulatedUART::workerFunction, this);
    }
    
    return true;
}

void SimulatedUART::deinit() {
    if (!initialized_) return;
    
    running_ = false;
    dataAvailable_.notify_all();
    
    if (workerThread_ && workerThread_->joinable()) {
        workerThread_->join();
    }
    
    initialized_ = false;
    Logger::log(LogLevel::INFO, "UART", "Deinitialized " + portName_);
}

bool SimulatedUART::sendByte(uint8_t data) {
    if (!initialized_) {
        Logger::log(LogLevel::ERROR, "UART", "UART not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    if (config_.parity != Parity::NONE) {
        data = addParity(data);
    }
    
    txBuffer_.push(data);
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.bytesSent++;
    }
    
    Logger::log(LogLevel::DEBUG, "UART", 
        "Sent byte: 0x" + std::to_string(data));
    
    if (config_.mode == UARTMode::POLLING) {
        // Immediate transmission simulation
        Logger::log(LogLevel::DEBUG, "UART", "Byte transmitted (polling mode)");
        if (txCallback_) {
            txCallback_(true);
        }
    }
    
    return true;
}

bool SimulatedUART::sendData(const std::vector<uint8_t>& data) {
    if (!initialized_) return false;
    
    for (uint8_t byte : data) {
        if (!sendByte(byte)) {
            return false;
        }
        // Simulate transmission delay based on baud rate
        std::this_thread::sleep_for(
            std::chrono::microseconds(1000000 / static_cast<int>(config_.baudRate) * 10)
        );
    }
    
    return true;
}

bool SimulatedUART::sendString(const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return sendData(data);
}

uint8_t SimulatedUART::readByte() {
    std::unique_lock<std::mutex> lock(bufferMutex_);
    
    if (rxBuffer_.empty()) {
        if (config_.mode == UARTMode::POLLING) {
            throw std::runtime_error("No data available");
        } else {
            dataAvailable_.wait(lock, [this] { 
                return !rxBuffer_.empty() || !running_; 
            });
            
            if (!running_) return 0;
        }
    }
    
    uint8_t data = rxBuffer_.front();
    rxBuffer_.pop();
    
    if (config_.parity != Parity::NONE) {
        if (!checkParity(data)) {
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.errors++;
            Logger::log(LogLevel::ERROR, "UART", "Parity error detected");
        }
        data &= 0x7F; // Remove parity bit
    }
    
    Logger::log(LogLevel::DEBUG, "UART", 
        "Read byte: 0x" + std::to_string(data));
    
    return data;
}

std::vector<uint8_t> SimulatedUART::readData(size_t size) {
    std::vector<uint8_t> data;
    for (size_t i = 0; i < size && !rxBuffer_.empty(); i++) {
        try {
            data.push_back(readByte());
        } catch (const std::exception& e) {
            break;
        }
    }
    return data;
}

std::string SimulatedUART::readString() {
    std::string result;
    while (!rxBuffer_.empty()) {
        uint8_t byte = readByte();
        if (byte == '\0' || byte == '\n') break;
        result += static_cast<char>(byte);
    }
    return result;
}

void SimulatedUART::registerRxCallback(RxCallback callback) {
    rxCallback_ = callback;
    Logger::log(LogLevel::INFO, "UART", "Registered RX callback");
}

void SimulatedUART::registerTxCallback(TxCallback callback) {
    txCallback_ = callback;
    Logger::log(LogLevel::INFO, "UART", "Registered TX callback");
}

bool SimulatedUART::isInitialized() const {
    return initialized_;
}

size_t SimulatedUART::availableData() const {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    return rxBuffer_.size();
}

void SimulatedUART::flushBuffer() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    while (!txBuffer_.empty()) txBuffer_.pop();
    while (!rxBuffer_.empty()) rxBuffer_.pop();
    Logger::log(LogLevel::DEBUG, "UART", "Buffers flushed");
}

SimulatedUART::Statistics SimulatedUART::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void SimulatedUART::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Statistics();
}

void SimulatedUART::simulateDataReception(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    for (uint8_t byte : data) {
        if (rxBuffer_.size() >= BUFFER_SIZE) {
            stats_.overruns++;
            Logger::log(LogLevel::ERROR, "UART", "Buffer overrun!");
            break;
        }
        
        if (config_.parity != Parity::NONE) {
            byte = addParity(byte);
        }
        
        rxBuffer_.push(byte);
        stats_.bytesReceived++;
    }
    
    dataAvailable_.notify_one();
    
    if (rxCallback_) {
        // Create a copy of data for callback
        std::vector<uint8_t> dataCopy = data;
        rxCallback_(dataCopy);
    }
    
    Logger::log(LogLevel::DEBUG, "UART", 
        "Simulated reception of " + std::to_string(data.size()) + " bytes");
}

void SimulatedUART::simulateParityError() {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.errors++;
    Logger::log(LogLevel::ERROR, "UART", "Simulated parity error");
}

void SimulatedUART::simulateOverrunError() {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.overruns++;
    Logger::log(LogLevel::ERROR, "UART", "Simulated overrun error");
}

void SimulatedUART::simulateFramingError() {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.errors++;
    Logger::log(LogLevel::ERROR, "UART", "Simulated framing error");
}

void SimulatedUART::workerFunction() {
    Logger::log(LogLevel::INFO, "UART", "Worker thread started");
    
    while (running_) {
        processTxData();
        processRxData();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::log(LogLevel::INFO, "UART", "Worker thread stopped");
}

void SimulatedUART::processTxData() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (!txBuffer_.empty()) {
        uint8_t data = txBuffer_.front();
        txBuffer_.pop();
        
        // Simulate transmission
        if (txCallback_) {
            txCallback_(true);
        }
        
        Logger::log(LogLevel::DEBUG, "UART", 
            "Transmitted byte in worker: 0x" + std::to_string(data));
    }
}

void SimulatedUART::processRxData() {
    // In a real system, this would check hardware registers
    // For simulation, data is added via simulateDataReception()
}

bool SimulatedUART::validateByte(uint8_t data) {
    if (config_.dataBits == DataBits::BITS_7) {
        return (data & 0x80) == 0;
    }
    return true;
}

uint8_t SimulatedUART::addParity(uint8_t data) {
    int count = 0;
    for (int i = 0; i < 7; i++) {
        if (data & (1 << i)) count++;
    }
    
    bool parityBit = (config_.parity == Parity::EVEN) ? 
                     (count % 2 == 0) : (count % 2 == 1);
    
    return data | (parityBit ? 0x80 : 0x00);
}

bool SimulatedUART::checkParity(uint8_t data) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (data & (1 << i)) count++;
    }
    
    bool expectedParity = (config_.parity == Parity::EVEN) ? 
                          (count % 2 == 0) : (count % 2 == 1);
    
    return expectedParity;
}