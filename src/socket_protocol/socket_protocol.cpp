#include "socket_protocol.hpp"

SocketProtocol::SocketProtocol()
    : serverSocket_(-1), clientSocket_(-1), 
      state_(ProtocolState::IDLE), isServer_(false), 
      connected_(false), running_(false), sequenceCounter_(0) {
    Logger::log(LogLevel::INFO, "PROTOCOL", "SocketProtocol created");
}

SocketProtocol::~SocketProtocol() {
    disconnect();
    stopServer();
    Logger::log(LogLevel::INFO, "PROTOCOL", "SocketProtocol destroyed");
}

bool SocketProtocol::startServer(const ProtocolConfig& config) {
    config_ = config;
    isServer_ = true;
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Failed to create server socket");
        return false;
    }
    
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(config_.port);
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Failed to bind server socket");
        close(serverSocket_);
        return false;
    }
    
    if (listen(serverSocket_, config_.maxClients) < 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Failed to listen on server socket");
        close(serverSocket_);
        return false;
    }
    
    Logger::log(LogLevel::INFO, "PROTOCOL", 
               "Server started on port " + std::to_string(config_.port));
    
    running_ = true;
    serverThread_ = std::make_unique<std::thread>(&SocketProtocol::serverLoop, this);
    
    return true;
}

void SocketProtocol::stopServer() {
    running_ = false;
    
    if (clientSocket_ >= 0) {
        close(clientSocket_);
        clientSocket_ = -1;
    }
    
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    
    Logger::log(LogLevel::INFO, "PROTOCOL", "Server stopped");
}

bool SocketProtocol::connectToServer(const std::string& ip, uint16_t port) {
    clientSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket_ < 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Failed to create client socket");
        return false;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Invalid server address");
        close(clientSocket_);
        return false;
    }
    
    if (connect(clientSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Connection failed");
        close(clientSocket_);
        clientSocket_ = -1;
        return false;
    }
    
    connected_ = true;
    isServer_ = false;
    Logger::log(LogLevel::INFO, "PROTOCOL", "Connected to server " + ip + ":" + std::to_string(port));
    
    receiveThread_ = std::make_unique<std::thread>(&SocketProtocol::receiveLoop, this);
    
    return true;
}

void SocketProtocol::disconnect() {
    running_ = false;
    connected_ = false;
    
    if (receiveThread_ && receiveThread_->joinable()) {
        receiveThread_->join();
    }
    
    if (clientSocket_ >= 0) {
        close(clientSocket_);
        clientSocket_ = -1;
    }
    
    Logger::log(LogLevel::INFO, "PROTOCOL", "Disconnected");
}

bool SocketProtocol::sendFrame(const Frame& frame) {
    if (!connected_) {
        Logger::log(LogLevel::ERROR, "PROTOCOL", "Not connected");
        return false;
    }
    
    std::vector<uint8_t> serialized = serializeFrame(frame);
    
    Logger::log(LogLevel::DEBUG, "PROTOCOL", 
               "Sending frame: type=" + std::to_string(frame.type) +
               " length=" + std::to_string(frame.length) +
               " seq=" + std::to_string(frame.sequence));
    
    return sendData(serialized);
}

SocketProtocol::Frame SocketProtocol::receiveFrame(std::chrono::milliseconds timeout) {
    Frame frame = {};
    std::vector<uint8_t> data;
    
    auto start = std::chrono::system_clock::now();
    
    while (std::chrono::system_clock::now() - start < timeout) {
        uint8_t byte;
        ssize_t bytesRead = recv(clientSocket_, &byte, 1, MSG_DONTWAIT);
        
        if (bytesRead > 0) {
            processByte(byte);
            
            if (state_ == ProtocolState::FRAME_READY) {
                frame = currentFrame_;
                resetReceiver();
                return frame;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    throw std::runtime_error("Receive timeout");
}

void SocketProtocol::registerFrameCallback(FrameCallback callback) {
    frameCallback_ = callback;
}

void SocketProtocol::registerErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

SocketProtocol::Frame SocketProtocol::createFrame(FrameType type, const std::vector<uint8_t>& data) {
    Frame frame;
    frame.startByte = Frame::START_BYTE;
    frame.length = data.size();
    frame.type = static_cast<uint8_t>(type);
    frame.sequence = 0; // Will be set before sending
    frame.payload = data;
    frame.crc = calculateCRC(data);
    frame.endByte = Frame::END_BYTE;
    
    return frame;
}

uint16_t SocketProtocol::calculateCRC(const std::vector<uint8_t>& data) {
    uint16_t crc = 0xFFFF;
    
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

bool SocketProtocol::validateFrame(const Frame& frame) {
    if (frame.startByte != Frame::START_BYTE) return false;
    if (frame.endByte != Frame::END_BYTE) return false;
    if (frame.length != frame.payload.size()) return false;
    
    uint16_t calculatedCrc = calculateCRC(frame.payload);
    if (calculatedCrc != frame.crc) return false;
    
    return true;
}

bool SocketProtocol::isConnected() const {
    return connected_;
}

SocketProtocol::ProtocolState SocketProtocol::getState() const {
    return state_;
}

std::string SocketProtocol::getStateString() const {
    switch(state_) {
        case ProtocolState::IDLE: return "IDLE";
        case ProtocolState::WAITING_START: return "WAITING_START";
        case ProtocolState::RECEIVING_HEADER: return "RECEIVING_HEADER";
        case ProtocolState::RECEIVING_PAYLOAD: return "RECEIVING_PAYLOAD";
        case ProtocolState::RECEIVING_CRC: return "RECEIVING_CRC";
        case ProtocolState::WAITING_END: return "WAITING_END";
        case ProtocolState::FRAME_READY: return "FRAME_READY";
        default: return "UNKNOWN";
    }
}

void SocketProtocol::serverLoop() {
    Logger::log(LogLevel::INFO, "PROTOCOL", "Server loop started");
    
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        // Set socket to non-blocking for accept
        int flags = fcntl(serverSocket_, F_GETFL, 0);
        fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK);
        
        int client = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (client >= 0) {
            Logger::log(LogLevel::INFO, "PROTOCOL", 
                       "New client connected: " + std::string(inet_ntoa(clientAddr.sin_addr)));
            
            if (clientSocket_ >= 0) {
                close(clientSocket_);
            }
            
            clientSocket_ = client;
            connected_ = true;
            
            receiveThread_ = std::make_unique<std::thread>(&SocketProtocol::receiveLoop, this);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Logger::log(LogLevel::INFO, "PROTOCOL", "Server loop stopped");
}

void SocketProtocol::receiveLoop() {
    Logger::log(LogLevel::INFO, "PROTOCOL", "Receive loop started");
    resetReceiver();
    
    while (running_ && connected_) {
        uint8_t byte;
        ssize_t bytesRead = recv(clientSocket_, &byte, 1, MSG_DONTWAIT);
        
        if (bytesRead > 0) {
            processByte(byte);
            
            if (state_ == ProtocolState::FRAME_READY) {
                Logger::log(LogLevel::DEBUG, "PROTOCOL", 
                           "Frame received: type=" + std::to_string(currentFrame_.type) +
                           " length=" + std::to_string(currentFrame_.length));
                
                if (frameCallback_) {
                    frameCallback_(currentFrame_);
                }
                
                // Send ACK if enabled
                if (config_.useAck && currentFrame_.type != static_cast<uint8_t>(FrameType::ACK)) {
                    Frame ackFrame = createFrame(FrameType::ACK, 
                        {currentFrame_.sequence});
                    sendFrame(ackFrame);
                }
                
                resetReceiver();
            }
        } else if (bytesRead == 0) {
            // Connection closed
            Logger::log(LogLevel::WARNING, "PROTOCOL", "Connection closed by peer");
            connected_ = false;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::log(LogLevel::INFO, "PROTOCOL", "Receive loop stopped");
}

void SocketProtocol::processByte(uint8_t byte) {
    switch(state_) {
        case ProtocolState::IDLE:
        case ProtocolState::WAITING_START:
            if (byte == Frame::START_BYTE) {
                currentFrame_ = Frame();
                currentFrame_.startByte = byte;
                state_ = ProtocolState::RECEIVING_HEADER;
                expectedBytes_ = sizeof(currentFrame_.length) + 
                                sizeof(currentFrame_.type) + 
                                sizeof(currentFrame_.sequence);
            }
            break;
            
        case ProtocolState::RECEIVING_HEADER:
            receiveBuffer_.push_back(byte);
            if (receiveBuffer_.size() == expectedBytes_) {
                // Parse header
                size_t idx = 0;
                memcpy(&currentFrame_.length, &receiveBuffer_[idx], sizeof(currentFrame_.length));
                idx += sizeof(currentFrame_.length);
                currentFrame_.type = receiveBuffer_[idx++];
                currentFrame_.sequence = receiveBuffer_[idx++];
                
                receiveBuffer_.clear();
                
                if (currentFrame_.length > 0) {
                    state_ = ProtocolState::RECEIVING_PAYLOAD;
                    expectedBytes_ = currentFrame_.length;
                } else {
                    state_ = ProtocolState::RECEIVING_CRC;
                    expectedBytes_ = sizeof(currentFrame_.crc);
                }
            }
            break;
            
        case ProtocolState::RECEIVING_PAYLOAD:
            receiveBuffer_.push_back(byte);
            if (receiveBuffer_.size() == expectedBytes_) {
                currentFrame_.payload = receiveBuffer_;
                receiveBuffer_.clear();
                
                state_ = ProtocolState::RECEIVING_CRC;
                expectedBytes_ = sizeof(currentFrame_.crc);
            }
            break;
            
        case ProtocolState::RECEIVING_CRC:
            receiveBuffer_.push_back(byte);
            if (receiveBuffer_.size() == expectedBytes_) {
                memcpy(&currentFrame_.crc, receiveBuffer_.data(), sizeof(currentFrame_.crc));
                receiveBuffer_.clear();
                
                state_ = ProtocolState::WAITING_END;
                expectedBytes_ = sizeof(currentFrame_.endByte);
            }
            break;
            
        case ProtocolState::WAITING_END:
            if (byte == Frame::END_BYTE) {
                currentFrame_.endByte = byte;
                
                if (validateFrame(currentFrame_)) {
                    state_ = ProtocolState::FRAME_READY;
                } else {
                    Logger::log(LogLevel::ERROR, "PROTOCOL", "Invalid frame received");
                    if (errorCallback_) {
                        errorCallback_("Frame validation failed");
                    }
                    resetReceiver();
                }
            } else {
                Logger::log(LogLevel::ERROR, "PROTOCOL", "Expected END_BYTE, got 0x" + 
                           std::to_string(byte));
                resetReceiver();
            }
            break;
            
        default:
            break;
    }
}

void SocketProtocol::resetReceiver() {
    state_ = ProtocolState::WAITING_START;
    receiveBuffer_.clear();
    expectedBytes_ = 0;
    currentFrame_ = Frame();
}

bool SocketProtocol::sendData(const std::vector<uint8_t>& data) {
    if (clientSocket_ < 0) return false;
    
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = send(clientSocket_, 
                           data.data() + totalSent, 
                           data.size() - totalSent, 
                           0);
        
        if (sent < 0) {
            Logger::log(LogLevel::ERROR, "PROTOCOL", "Failed to send data");
            return false;
        }
        
        totalSent += sent;
    }
    
    return true;
}

std::vector<uint8_t> SocketProtocol::serializeFrame(const Frame& frame) {
    std::vector<uint8_t> data;
    
    // Start byte
    data.push_back(frame.startByte);
    
    // Length
    data.push_back(frame.length & 0xFF);
    data.push_back((frame.length >> 8) & 0xFF);
    
    // Type
    data.push_back(frame.type);
    
    // Sequence
    data.push_back(frame.sequence);
    
    // Payload
    data.insert(data.end(), frame.payload.begin(), frame.payload.end());
    
    // CRC
    data.push_back(frame.crc & 0xFF);
    data.push_back((frame.crc >> 8) & 0xFF);
    
    // End byte
    data.push_back(frame.endByte);
    
    return data;
}

bool SocketProtocol::deserializeFrame(const std::vector<uint8_t>& data, Frame& frame) {
    if (data.size() < 7) return false; // Minimum frame size
    
    size_t idx = 0;
    
    // Start byte
    frame.startByte = data[idx++];
    if (frame.startByte != Frame::START_BYTE) return false;
    
    // Length
    frame.length = data[idx++] | (data[idx++] << 8);
    
    // Type
    frame.type = data[idx++];
    
    // Sequence
    frame.sequence = data[idx++];
    
    // Payload
    if (idx + frame.length > data.size() - 3) return false;
    frame.payload.assign(data.begin() + idx, data.begin() + idx + frame.length);
    idx += frame.length;
    
    // CRC
    frame.crc = data[idx++] | (data[idx++] << 8);
    
    // End byte
    frame.endByte = data[idx++];
    
    return validateFrame(frame);
}