#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "NetworkTypes.h"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <map>

/**
 * Completed LiDAR scan data structure
 */
struct CompletedLidarScan {
    double timestamp;
    int roverId;
    std::vector<LidarPoint> points;
};

/**
 * Network state for a single rover
 */
struct RoverNetworkState {
    int roverId;
    
    // Network sockets
    int poseSocket;
    int lidarSocket;
    int telemSocket;
    int cmdSocket;
    
    // Latest data
    PosePacket latestPose;
    VehicleTelem latestTelem;
    
    // LiDAR assembly state
    std::map<double, LidarAssemblyState> lidarAssembly;
    
    // Network statistics
    NetworkStats stats;
    
    // Status
    std::atomic<bool> online;
    std::chrono::high_resolution_clock::time_point lastPacketTime;
    
    // Thread synchronization
    mutable std::mutex dataMutex;
    
    explicit RoverNetworkState(int id) : roverId(id), poseSocket(-1), lidarSocket(-1), 
                                        telemSocket(-1), cmdSocket(-1), latestPose{}, 
                                        latestTelem{}, stats{}, online(false),
                                        lastPacketTime(std::chrono::high_resolution_clock::now()) {
    }
};

/**
 * High-performance multi-rover network management system
 * Handles UDP communication for pose, LiDAR, and telemetry data
 */
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    /**
     * Initialize the network manager and start receive threads
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * Shutdown the network manager and cleanup resources
     */
    void shutdown();
    
    /**
     * Update network statistics and process commands
     * Should be called regularly from the main thread
     */
    void update();
    
    /**
     * Send a button command to a specific rover
     * @param roverId target rover ID
     * @param buttonMask button state mask
     * @return true if command was queued successfully
     */
    bool sendButtonCommand(int roverId, uint8_t buttonMask);
    
    /**
     * Get the latest pose data for a rover
     * @param roverId rover ID
     * @return pointer to latest pose data, or nullptr if not available
     */
    const PosePacket* getLatestPose(int roverId) const;
    
    /**
     * Get the latest telemetry data for a rover
     * @param roverId rover ID
     * @return pointer to latest telemetry data, or nullptr if not available
     */
    const VehicleTelem* getLatestTelemetry(int roverId) const;
    
    /**
     * Get all completed LiDAR scans for a rover
     * @param roverId rover ID
     * @param outScans output vector to store completed scans
     */
    void getCompletedLidarScans(int roverId, std::vector<CompletedLidarScan>& outScans);
    
    /**
     * Get network statistics for a rover
     * @param roverId rover ID
     * @return network statistics structure
     */
    NetworkStats getNetworkStats(int roverId) const;
    
    /**
     * Check if a rover is currently online
     * @param roverId rover ID
     * @return true if rover is online
     */
    bool isRoverOnline(int roverId) const;
    
    /**
     * Get the total number of LiDAR points received
     * @return total point count
     */
    size_t getTotalPointCount() const;

private:
    // Rover network states
    std::vector<std::unique_ptr<RoverNetworkState>> m_roverStates;
    
    // Network threads
    std::vector<std::thread> m_receiveThreads;
    std::atomic<bool> m_running;
    
    // Command queue
    std::queue<std::pair<int, uint8_t>> m_commandQueue;
    std::mutex m_commandMutex;
    
    // Statistics
    std::atomic<size_t> m_totalPointsReceived;
    
    // Socket management
    int createUDPSocket(int port, bool blocking);
    
    // Thread functions
    void poseReceiveThread(int roverId);
    void lidarReceiveThread(int roverId);
    void telemReceiveThread(int roverId);
    
    // Packet processing
    void processPosePacket(RoverNetworkState& state, const PosePacket& packet);
    void processLidarPacket(RoverNetworkState& state, const LidarPacket& packet);
    void processTelemPacket(RoverNetworkState& state, const VehicleTelem& packet);
    
    // LiDAR assembly
    void assembleLidarScans(RoverNetworkState& state);
    void cleanupExpiredLidarStates(RoverNetworkState& state);
    
    // Statistics and diagnostics
    void updateNetworkStats(RoverNetworkState& state, PacketType packetType);
    
    // Command processing
    void processCommandQueue();
    
    // Utility functions
    ssize_t sendUDP(int socket, const void* data, size_t size, int port);
    double getCurrentTime() const;
    RoverNetworkState* getRoverState(int roverId);
    const RoverNetworkState* getRoverState(int roverId) const;
    bool isValidRoverId(int roverId) const;
};

#endif // NETWORK_MANAGER_H