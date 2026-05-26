#ifndef SOCKET_PROTOCOL_HPP
#define SOCKET_PROTOCOL_HPP

#include "common.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

class SocketProtocol {
public:
    // Protocol frame structure
    struct Frame {
        static const uint8_t START_BYTE = 0xAA;
        static const uint8_t END_BYTE = 0x55;
        
        uint8_t startByte;
        uint16_t length;
        uint8_t type;
        uint8_t sequence;
        std::vector<uint8_t> payload;
        uint16_t crc;
        uint8_t endByte;
    };
    
    enum class FrameType : uint8_t {
        DATA = 0x01,
        ACK = 0x02,
        NACK = 0x03,
        HEARTBEAT = 0x04,
        COMMAND = 0x05,
        RESPONSE = 0x06
    };
    
    enum class ProtocolState {
        IDLE,
        WAITING_START,
        RECEIVING_HEADER,
        RECEIVING_PAYLOAD,
        RECEIVING_CRC,
        WAITING_END,
        FRAME_READY
    };
    
    struct ProtocolConfig {
        uint16_t port = 8080;
        int maxClients = 5;
        size_t bufferSize = 1024;
        bool useAck = true;
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
        std::chrono::milliseconds heartbeatInterval = std::chrono::milliseconds(1000);
    };
    
    // Callbacks
    using FrameCallback = std::function<void(const Frame&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    SocketProtocol();
    ~SocketProtocol();
    
    // Server operations
    bool startServer(const ProtocolConfig& config);
    void stopServer();
    
    // Client operations
    bool connectToServer(const std::string& ip, uint16_t port);
    void disconnect();
    
    // Data transfer
    bool sendFrame(const Frame& frame);
    Frame receiveFrame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    
    // Callback registration
    void registerFrameCallback(FrameCallback callback);
    void registerErrorCallback(ErrorCallback callback);
    
    // Utilities
    static Frame createFrame(FrameType type, const std::vector<uint8_t>& data);
    static uint16_t calculateCRC(const std::vector<uint8_t>& data);
    static bool validateFrame(const Frame& frame);
    
    // Status
    bool isConnected() const;
    ProtocolState getState() const;
    std::string getStateString() const;
    
private:
    int serverSocket_;
    int clientSocket_;
    ProtocolConfig config_;
    ProtocolState state_;
    bool isServer_;
    bool connected_;
    
    // Threading
    std::unique_ptr<std::thread> serverThread_;
    std::unique_ptr<std::thread> receiveThread_;
    std::atomic<bool> running_;
    
    // Callbacks
    FrameCallback frameCallback_;
    ErrorCallback errorCallback_;
    
    // Buffer management
    std::vector<uint8_t> receiveBuffer_;
    std::mutex bufferMutex_;
    
    // Frame assembly
    Frame currentFrame_;
    size_t expectedBytes_;
    uint8_t sequenceCounter_;
    
    void serverLoop();
    void receiveLoop();
    void processByte(uint8_t byte);
    void resetReceiver();
    bool sendData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeFrame(const Frame& frame);
    bool deserializeFrame(const std::vector<uint8_t>& data, Frame& frame);
};

#endif // SOCKET_PROTOCOL_HPP