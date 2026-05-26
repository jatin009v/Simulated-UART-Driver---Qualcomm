#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <memory>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <map>
#include <atomic>

#define BUFFER_SIZE 256
#define MAX_TASKS 10
#define MAX_CLIENTS 5

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void log(LogLevel level, const std::string& module, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string levelStr;
        switch(level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO: levelStr = "INFO"; break;
            case LogLevel::WARNING: levelStr = "WARN"; break;
            case LogLevel::ERROR: levelStr = "ERROR"; break;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] "
                  << "[" << levelStr << "] "
                  << "[" << module << "] "
                  << message << std::endl;
    }

private:
    static std::mutex mutex_;
};

std::mutex Logger::mutex_;

#endif // COMMON_HPP