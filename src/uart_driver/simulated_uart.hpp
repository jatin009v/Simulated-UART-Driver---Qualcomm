#ifndef SIMULATED_UART_HPP
#define SIMULATED_UART_HPP

#include "common.hpp"

class SimulatedUART {
public:
    enum class UARTMode {
        POLLING,
        INTERRUPT,
        DMA
    };
    
    enum class BaudRate {
        B9600 = 9600,
        B19200 = 19200,
        B38400 = 38400,
        B57600 = 57600,
        B115200 = 115200
    };
    
    enum class DataBits {
        BITS_7 = 7,
        BITS_8 = 8
    };
    
    enum class Parity {
        NONE,
        EVEN,
        ODD
    };
    
    enum class StopBits {
        BITS_1 = 1,
        BITS_2 = 2
    };
    
    struct UARTConfig {
        BaudRate baudRate = BaudRate::B115200;
        DataBits dataBits = DataBits::BITS_8;
        Parity parity = Parity::NONE;
        StopBits stopBits = StopBits::BITS_1;
        UARTMode mode = UARTMode::INTERRUPT;
    };
    
    // Callback type for received data
    using RxCallback = std::function<void(const std::vector<uint8_t>&)>;
    using TxCallback = std::function<void(bool)>;
    
    SimulatedUART(const std::string& portName = "UART0");
    ~SimulatedUART();
    
    // Initialization
    bool init(const UARTConfig& config);
    void deinit();
    
    // Data transfer
    bool sendByte(uint8_t data);
    bool sendData(const std::vector<uint8_t>& data);
    bool sendString(const std::string& str);
    
    // Reception
    uint8_t readByte();
    std::vector<uint8_t> readData(size_t size);
    std::string readString();
    
    // Callback registration
    void registerRxCallback(RxCallback callback);
    void registerTxCallback(TxCallback callback);
    
    // Status
    bool isInitialized() const;
    size_t availableData() const;
    void flushBuffer();
    
    // Statistics
    struct Statistics {
        size_t bytesSent = 0;
        size_t bytesReceived = 0;
        size_t errors = 0;
        size_t overruns = 0;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Simulated hardware events
    void simulateDataReception(const std::vector<uint8_t>& data);
    void simulateParityError();
    void simulateOverrunError();
    void simulateFramingError();

private:
    std::string portName_;
    UARTConfig config_;
    bool initialized_;
    
    // Buffers
    std::queue<uint8_t> txBuffer_;
    std::queue<uint8_t> rxBuffer_;
    std::mutex bufferMutex_;
    std::condition_variable dataAvailable_;
    
    // Callbacks
    RxCallback rxCallback_;
    TxCallback txCallback_;
    
    // Statistics
    Statistics stats_;
    mutable std::mutex statsMutex_;
    
    // Worker thread for interrupt mode
    std::unique_ptr<std::thread> workerThread_;
    std::atomic<bool> running_;
    
    void workerFunction();
    void processRxData();
    void processTxData();
    bool validateByte(uint8_t data);
    uint8_t addParity(uint8_t data);
    bool checkParity(uint8_t data);
};

#endif // SIMULATED_UART_HPP