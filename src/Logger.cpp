#include "Logger.h"
#include "Constants.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Logger {
    
    SystemLogger::SystemLogger()
        : currentLogLevel_(LogLevel::INFO)
        , logToFile_(false)
        , logToConsole_(true)
        , shouldStop_(false) {
        
        // Initialize message counters
        for (auto& count : messageCounts_) {
            count.store(0);
        }
        
        // Start background logging thread
        logThread_ = std::thread(&SystemLogger::processLogMessages, this);
        
        LOG_INFO("Logger initialized successfully");
    }
    
    SystemLogger::~SystemLogger() {
        LOG_INFO("Logger shutting down");
        
        // Signal thread to stop
        shouldStop_.store(true);
        logCondition_.notify_all();
        
        // Wait for thread to finish
        if (logThread_.joinable()) {
            logThread_.join();
        }
        
        // Close log file if open
        if (logFile_ && logFile_->is_open()) {
            logFile_->close();
        }
    }
    
    void SystemLogger::log(LogLevel level, const std::string& message, const std::string& file, int line) {
        if (level < currentLogLevel_) {
            return; // Filter out messages below current log level
        }
        
        // Create log message
        LogMessage logMsg(level, message, file, line);
        
        // Add to queue for background processing
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.push(logMsg);
        }
        
        // Increment message counter
        if (static_cast<size_t>(level) < messageCounts_.size()) {
            messageCounts_[static_cast<size_t>(level)]++;
        }
        
        // Notify processing thread
        logCondition_.notify_one();
    }
    
    void SystemLogger::trace(const std::string& message, const std::string& file, int line) {
        log(LogLevel::TRACE, message, file, line);
    }
    
    void SystemLogger::debug(const std::string& message, const std::string& file, int line) {
        log(LogLevel::DEBUG, message, file, line);
    }
    
    void SystemLogger::info(const std::string& message, const std::string& file, int line) {
        log(LogLevel::INFO, message, file, line);
    }
    
    void SystemLogger::warn(const std::string& message, const std::string& file, int line) {
        log(LogLevel::WARN, message, file, line);
    }
    
    void SystemLogger::error(const std::string& message, const std::string& file, int line) {
        log(LogLevel::ERROR, message, file, line);
    }
    
    void SystemLogger::fatal(const std::string& message, const std::string& file, int line) {
        log(LogLevel::FATAL, message, file, line);
    }
    
    void SystemLogger::recordMetric(MetricType type, double value, const std::string& context) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        MetricSample sample(value, context);
        metrics_[type].push(sample);
        
        // Clean old metrics if queue is too large
        cleanOldMetrics(type);
    }
    
    void SystemLogger::recordLatency(MetricType type, const std::chrono::high_resolution_clock::time_point& start) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        recordMetric(type, duration.count() / 1000.0); // Convert to milliseconds
    }
    
    std::vector<MetricSample> SystemLogger::getMetricHistory(MetricType type, size_t maxSamples) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        std::vector<MetricSample> samples;
        auto& queue = metrics_[type];
        
        // Convert queue to vector (most recent samples first)
        std::queue<MetricSample> tempQueue = queue;
        while (!tempQueue.empty() && samples.size() < maxSamples) {
            samples.push_back(tempQueue.front());
            tempQueue.pop();
        }
        
        // Reverse to get chronological order
        std::reverse(samples.begin(), samples.end());
        
        return samples;
    }
    
    double SystemLogger::getAverageMetric(MetricType type, std::chrono::milliseconds timeWindow) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        auto now = std::chrono::high_resolution_clock::now();
        auto cutoff = now - timeWindow;
        
        auto& queue = metrics_[type];
        double sum = 0.0;
        size_t count = 0;
        
        std::queue<MetricSample> tempQueue = queue;
        while (!tempQueue.empty()) {
            const auto& sample = tempQueue.front();
            if (sample.timestamp >= cutoff) {
                sum += sample.value;
                count++;
            }
            tempQueue.pop();
        }
        
        return count > 0 ? sum / count : 0.0;
    }
    
    double SystemLogger::getMaxMetric(MetricType type, std::chrono::milliseconds timeWindow) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        auto now = std::chrono::high_resolution_clock::now();
        auto cutoff = now - timeWindow;
        
        auto& queue = metrics_[type];
        double maxValue = std::numeric_limits<double>::lowest();
        bool found = false;
        
        std::queue<MetricSample> tempQueue = queue;
        while (!tempQueue.empty()) {
            const auto& sample = tempQueue.front();
            if (sample.timestamp >= cutoff) {
                maxValue = std::max(maxValue, sample.value);
                found = true;
            }
            tempQueue.pop();
        }
        
        return found ? maxValue : 0.0;
    }
    
    double SystemLogger::getMinMetric(MetricType type, std::chrono::milliseconds timeWindow) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        
        auto now = std::chrono::high_resolution_clock::now();
        auto cutoff = now - timeWindow;
        
        auto& queue = metrics_[type];
        double minValue = std::numeric_limits<double>::max();
        bool found = false;
        
        std::queue<MetricSample> tempQueue = queue;
        while (!tempQueue.empty()) {
            const auto& sample = tempQueue.front();
            if (sample.timestamp >= cutoff) {
                minValue = std::min(minValue, sample.value);
                found = true;
            }
            tempQueue.pop();
        }
        
        return found ? minValue : 0.0;
    }
    
    void SystemLogger::setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(logMutex_);
        currentLogLevel_ = level;
        LOG_INFO("Log level changed to " + logLevelToString(level));
    }
    
    void SystemLogger::setLogToFile(bool enable, const std::string& filename) {
        std::lock_guard<std::mutex> lock(logMutex_);
        
        if (enable) {
            std::string logFileName = filename.empty() ? "lidar_visualization.log" : filename;
            
            // Create logs directory if it doesn't exist
            std::filesystem::create_directories(Constants::Paths::LOG_DIRECTORY);
            
            std::string fullPath = std::string(Constants::Paths::LOG_DIRECTORY) + logFileName;
            logFile_ = std::make_unique<std::ofstream>(fullPath, std::ios::app);
            
            if (logFile_->is_open()) {
                logToFile_ = true;
                LOG_INFO("Logging to file enabled: " + fullPath);
            } else {
                LOG_ERROR("Failed to open log file: " + fullPath);
            }
        } else {
            if (logFile_) {
                logFile_->close();
                logFile_.reset();
            }
            logToFile_ = false;
            LOG_INFO("Logging to file disabled");
        }
    }
    
    void SystemLogger::setLogToConsole(bool enable) {
        std::lock_guard<std::mutex> lock(logMutex_);
        logToConsole_ = enable;
        LOG_INFO(std::string("Console logging ") + (enable ? "enabled" : "disabled"));
    }
    
    size_t SystemLogger::getMessageCount(LogLevel level) const {
        if (static_cast<size_t>(level) < messageCounts_.size()) {
            return messageCounts_[static_cast<size_t>(level)].load();
        }
        return 0;
    }
    
    size_t SystemLogger::getTotalMessageCount() const {
        size_t total = 0;
        for (const auto& count : messageCounts_) {
            total += count.load();
        }
        return total;
    }
    
    void SystemLogger::clearMetrics() {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        metrics_.clear();
        LOG_INFO("Performance metrics cleared");
    }
    
    void SystemLogger::processLogMessages() {
        while (!shouldStop_.load()) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // Wait for messages or stop signal
            logCondition_.wait(lock, [this] { 
                return !messageQueue_.empty() || shouldStop_.load(); 
            });
            
            // Process all available messages
            while (!messageQueue_.empty()) {
                LogMessage msg = messageQueue_.front();
                messageQueue_.pop();
                lock.unlock();
                
                writeMessage(msg);
                
                lock.lock();
            }
        }
        
        // Process any remaining messages
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!messageQueue_.empty()) {
            writeMessage(messageQueue_.front());
            messageQueue_.pop();
        }
    }
    
    void SystemLogger::writeMessage(const LogMessage& msg) {
        std::string formattedMsg = formatMessage(msg);
        
        // Write to console if enabled
        if (logToConsole_) {
            if (msg.level >= LogLevel::ERROR) {
                std::cerr << formattedMsg << std::endl;
            } else {
                std::cout << formattedMsg << std::endl;
            }
        }
        
        // Write to file if enabled
        if (logToFile_ && logFile_ && logFile_->is_open()) {
            std::lock_guard<std::mutex> lock(logMutex_);
            *logFile_ << formattedMsg << std::endl;
            logFile_->flush(); // Ensure immediate write
        }
    }
    
    std::string SystemLogger::formatMessage(const LogMessage& msg) {
        std::ostringstream ss;
        
        // Timestamp
        auto time_t = std::chrono::system_clock::to_time_t(
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                msg.timestamp - std::chrono::high_resolution_clock::now() + 
                std::chrono::system_clock::now()
            )
        );
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.timestamp.time_since_epoch()
        ) % 1000;
        
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        // Log level
        ss << " [" << logLevelToString(msg.level) << "]";
        
        // Thread ID
        ss << " [" << msg.threadId << "]";
        
        // File and line (if provided)
        if (!msg.file.empty()) {
            std::string filename = std::filesystem::path(msg.file).filename().string();
            ss << " (" << filename << ":" << msg.line << ")";
        }
        
        // Message
        ss << " " << msg.message;
        
        return ss.str();
    }
    
    std::string SystemLogger::logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }
    
    void SystemLogger::cleanOldMetrics(MetricType type) {
        auto& queue = metrics_[type];
        auto now = std::chrono::high_resolution_clock::now();
        auto cutoff = now - METRIC_RETENTION_TIME;
        
        // Remove old samples
        while (!queue.empty() && queue.front().timestamp < cutoff) {
            queue.pop();
        }
        
        // Limit queue size
        while (queue.size() > MAX_METRIC_HISTORY) {
            queue.pop();
        }
    }
    
    // ScopedTimer implementation
    SystemLogger::ScopedTimer::ScopedTimer(SystemLogger& logger, MetricType metric, const std::string& context)
        : logger_(logger), metric_(metric), context_(context)
        , start_(std::chrono::high_resolution_clock::now()) {
    }
    
    SystemLogger::ScopedTimer::~ScopedTimer() {
        logger_.recordLatency(metric_, start_);
    }
    
    // Global instance
    SystemLogger& getInstance() {
        static SystemLogger instance;
        return instance;
    }
    
} // namespace Logger