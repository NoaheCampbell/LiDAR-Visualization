#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <queue>
#include <fstream>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <array>

/**
 * Thread-safe logging system with performance metrics tracking
 * Supports diagnostics panel requirements for Multi-Rover LiDAR Visualization
 */

namespace Logger {
    
    /**
     * Log levels for message filtering
     */
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    
    /**
     * Performance metric types for tracking system performance
     */
    enum class MetricType {
        NETWORK_LATENCY,        // Network packet processing time
        LIDAR_ASSEMBLY_TIME,    // LiDAR point cloud assembly time
        RENDER_FRAME_TIME,      // Frame rendering time
        UI_UPDATE_TIME,         // UI update time
        TOTAL_FRAME_TIME,       // Complete frame processing time
        PACKET_LOSS_RATE,       // Network packet loss percentage
        ROVER_CONNECTION_STATUS, // Rover connection health
        MEMORY_USAGE,           // System memory usage
        CPU_USAGE               // CPU utilization
    };
    
    /**
     * Performance metric sample
     */
    struct MetricSample {
        std::chrono::high_resolution_clock::time_point timestamp;
        double value;
        std::string context; // Optional context information
        
        MetricSample(double val, const std::string& ctx = "")
            : timestamp(std::chrono::high_resolution_clock::now()), value(val), context(ctx) {}
    };
    
    /**
     * Log message structure
     */
    struct LogMessage {
        std::chrono::high_resolution_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::string file;
        int line;
        std::thread::id threadId;
        
        LogMessage(LogLevel lvl, const std::string& msg, const std::string& f, int l)
            : timestamp(std::chrono::high_resolution_clock::now())
            , level(lvl), message(msg), file(f), line(l)
            , threadId(std::this_thread::get_id()) {}
    };
    
    /**
     * Main logger class providing thread-safe logging and metrics
     */
    class SystemLogger {
    public:
        SystemLogger();
        ~SystemLogger();
        
        // Logging functions
        void log(LogLevel level, const std::string& message, const std::string& file = "", int line = 0);
        void trace(const std::string& message, const std::string& file = "", int line = 0);
        void debug(const std::string& message, const std::string& file = "", int line = 0);
        void info(const std::string& message, const std::string& file = "", int line = 0);
        void warn(const std::string& message, const std::string& file = "", int line = 0);
        void error(const std::string& message, const std::string& file = "", int line = 0);
        void fatal(const std::string& message, const std::string& file = "", int line = 0);
        
        // Performance metrics
        void recordMetric(MetricType type, double value, const std::string& context = "");
        void recordLatency(MetricType type, const std::chrono::high_resolution_clock::time_point& start);
        
        // Metric retrieval for diagnostics
        std::vector<MetricSample> getMetricHistory(MetricType type, size_t maxSamples = 100);
        double getAverageMetric(MetricType type, std::chrono::milliseconds timeWindow = std::chrono::milliseconds(1000));
        double getMaxMetric(MetricType type, std::chrono::milliseconds timeWindow = std::chrono::milliseconds(1000));
        double getMinMetric(MetricType type, std::chrono::milliseconds timeWindow = std::chrono::milliseconds(1000));
        
        // Configuration
        void setLogLevel(LogLevel level);
        void setLogToFile(bool enable, const std::string& filename = "");
        void setLogToConsole(bool enable);
        
        // Statistics
        size_t getMessageCount(LogLevel level) const;
        size_t getTotalMessageCount() const;
        void clearMetrics();
        
        // Utility for timing operations
        class ScopedTimer {
        public:
            ScopedTimer(SystemLogger& logger, MetricType metric, const std::string& context = "");
            ~ScopedTimer();
            
        private:
            SystemLogger& logger_;
            MetricType metric_;
            std::string context_;
            std::chrono::high_resolution_clock::time_point start_;
        };
        
    private:
        mutable std::mutex logMutex_;
        mutable std::mutex metricsMutex_;
        
        LogLevel currentLogLevel_;
        bool logToFile_;
        bool logToConsole_;
        std::unique_ptr<std::ofstream> logFile_;
        
        // Background logging thread
        std::thread logThread_;
        std::atomic<bool> shouldStop_;
        std::queue<LogMessage> messageQueue_;
        std::condition_variable logCondition_;
        mutable std::mutex queueMutex_;
        
        // Metrics storage
        std::unordered_map<MetricType, std::queue<MetricSample>> metrics_;
        std::array<std::atomic<size_t>, 6> messageCounts_; // Count for each log level
        
        // Private methods
        void processLogMessages();
        void writeMessage(const LogMessage& msg);
        std::string formatMessage(const LogMessage& msg);
        std::string logLevelToString(LogLevel level);
        void cleanOldMetrics(MetricType type);
        
        static constexpr size_t MAX_METRIC_HISTORY = 10000;
        static constexpr std::chrono::minutes METRIC_RETENTION_TIME{5};
    };
    
    // Global logger instance
    SystemLogger& getInstance();
    
    // Convenience macros for logging with file/line information
    #define LOG_TRACE(msg) Logger::getInstance().trace(msg, __FILE__, __LINE__)
    #define LOG_DEBUG(msg) Logger::getInstance().debug(msg, __FILE__, __LINE__)
    #define LOG_INFO(msg) Logger::getInstance().info(msg, __FILE__, __LINE__)
    #define LOG_WARN(msg) Logger::getInstance().warn(msg, __FILE__, __LINE__)
    #define LOG_ERROR(msg) Logger::getInstance().error(msg, __FILE__, __LINE__)
    #define LOG_FATAL(msg) Logger::getInstance().fatal(msg, __FILE__, __LINE__)
    
    // Convenience macros for performance timing
    #define TIME_METRIC(metric, context) Logger::SystemLogger::ScopedTimer timer(Logger::getInstance(), metric, context)
    #define RECORD_METRIC(metric, value) Logger::getInstance().recordMetric(metric, value)
    #define RECORD_LATENCY(metric, start) Logger::getInstance().recordLatency(metric, start)
    
} // namespace Logger