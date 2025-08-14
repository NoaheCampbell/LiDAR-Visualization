#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>

/**
 * Log levels for filtering and categorizing messages
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4
};

/**
 * High-performance thread-safe logging system
 * Supports multiple output targets and configurable log levels
 */
class Logger {
public:
    /**
     * Initialize the logging system
     * @param level minimum log level to output
     * @param logToConsole whether to output to console
     * @param logFile optional file path for file logging
     * @return true if initialization successful
     */
    static bool initialize(LogLevel level = LogLevel::INFO, 
                          bool logToConsole = true, 
                          const std::string& logFile = "");
    
    /**
     * Shutdown the logging system and flush all outputs
     */
    static void shutdown();
    
    /**
     * Set the minimum log level
     * @param level new minimum log level
     */
    static void setLogLevel(LogLevel level);
    
    /**
     * Get the current minimum log level
     * @return current log level
     */
    static LogLevel getLogLevel();
    
    /**
     * Check if a log level would be output
     * @param level log level to check
     * @return true if this level would be logged
     */
    static bool isEnabled(LogLevel level);
    
    // Logging methods with format string support
    template<typename... Args>
    static void trace(const std::string& format, Args&&... args) {
        log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void debug(const std::string& format, Args&&... args) {
        log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(const std::string& format, Args&&... args) {
        log(LogLevel::INFO, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warn(const std::string& format, Args&&... args) {
        log(LogLevel::WARN, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(const std::string& format, Args&&... args) {
        log(LogLevel::ERROR, format, std::forward<Args>(args)...);
    }
    
    // Simple string logging methods
    static void trace(const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

private:
    static std::mutex s_mutex;
    static LogLevel s_level;
    static bool s_logToConsole;
    static std::unique_ptr<std::ofstream> s_logFile;
    static bool s_initialized;
    
    // Core logging implementation
    static void logMessage(LogLevel level, const std::string& message);
    
    // Format timestamp
    static std::string getTimestamp();
    
    // Get log level string
    static std::string getLevelString(LogLevel level);
    
    // Template implementation for formatted logging
    template<typename... Args>
    static void log(LogLevel level, const std::string& format, Args&&... args) {
        if (!isEnabled(level)) {
            return;
        }
        
        try {
            std::string message = formatString(format, std::forward<Args>(args)...);
            logMessage(level, message);
        } catch (const std::exception& e) {
            // Fallback logging if formatting fails
            logMessage(LogLevel::ERROR, "Logger formatting error: " + std::string(e.what()) + " (original: " + format + ")");
        }
    }
    
    // Simple format string implementation
    template<typename T>
    static std::string formatString(const std::string& format, T&& value) {
        std::ostringstream oss;
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << value << format.substr(pos + 2);
        } else {
            oss << format;
        }
        return oss.str();
    }
    
    template<typename T, typename... Args>
    static std::string formatString(const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            std::string prefix = format.substr(0, pos);
            std::string suffix = format.substr(pos + 2);
            std::ostringstream oss;
            oss << prefix << value;
            return oss.str() + formatString(suffix, std::forward<Args>(args)...);
        }
        return format;
    }
    
    static std::string formatString(const std::string& format) {
        return format;
    }
};

#endif // LOGGER_H