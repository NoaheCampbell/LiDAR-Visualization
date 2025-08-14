#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

// Static member definitions
std::mutex Logger::s_mutex;
LogLevel Logger::s_level = LogLevel::INFO;
bool Logger::s_logToConsole = true;
std::unique_ptr<std::ofstream> Logger::s_logFile = nullptr;
bool Logger::s_initialized = false;

bool Logger::initialize(LogLevel level, bool logToConsole, const std::string& logFile) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    s_level = level;
    s_logToConsole = logToConsole;
    
    // Initialize file logging if requested
    if (!logFile.empty()) {
        s_logFile = std::make_unique<std::ofstream>(logFile, std::ios::app);
        if (!s_logFile->is_open()) {
            std::cerr << "Error: Failed to open log file: " << logFile << std::endl;
            s_logFile.reset();
            return false;
        }
    }
    
    s_initialized = true;
    
    // Log initialization message
    logMessage(LogLevel::INFO, "Logger initialized - Level: " + getLevelString(level) + 
               ", Console: " + (logToConsole ? "enabled" : "disabled") +
               ", File: " + (s_logFile ? logFile : "disabled"));
    
    return true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized) {
        logMessage(LogLevel::INFO, "Logger shutting down");
        
        if (s_logFile) {
            s_logFile->flush();
            s_logFile->close();
            s_logFile.reset();
        }
        
        s_initialized = false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_level = level;
}

LogLevel Logger::getLogLevel() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_level;
}

bool Logger::isEnabled(LogLevel level) {
    return level >= s_level;
}

void Logger::trace(const std::string& message) {
    logMessage(LogLevel::TRACE, message);
}

void Logger::debug(const std::string& message) {
    logMessage(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    logMessage(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    logMessage(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    logMessage(LogLevel::ERROR, message);
}

void Logger::logMessage(LogLevel level, const std::string& message) {
    if (!isEnabled(level)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_initialized) {
        // Emergency fallback - log to stderr
        std::cerr << "[UNINIT] " << getLevelString(level) << ": " << message << std::endl;
        return;
    }
    
    std::string timestamp = getTimestamp();
    std::string levelStr = getLevelString(level);
    std::string fullMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Log to console if enabled
    if (s_logToConsole) {
        if (level >= LogLevel::WARN) {
            std::cerr << fullMessage << std::endl;
        } else {
            std::cout << fullMessage << std::endl;
        }
    }
    
    // Log to file if enabled
    if (s_logFile && s_logFile->is_open()) {
        *s_logFile << fullMessage << std::endl;
        s_logFile->flush();  // Ensure immediate write for debugging
    }
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}