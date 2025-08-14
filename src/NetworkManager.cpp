#include "NetworkManager.h"
#include "Logger.h"
#include "Constants.h"
#include "RoverProfiles.h"
#include "ButtonUtils.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <algorithm>

NetworkManager::NetworkManager() {
    Logger::debug("NetworkManager created");
}

NetworkManager::~NetworkManager() {
    shutdown();
    Logger::debug("NetworkManager destroyed");
}

bool NetworkManager::initialize() {
    Logger::info("Initializing NetworkManager...");
    
    // Validate rover profiles
    if (!RoverProfiles::validateProfiles()) {
        Logger::error("Invalid rover profiles configuration");
        return false;
    }
    
    // Initialize rover states
    m_roverStates.clear();
    int roverCount = RoverProfiles::getRoverCount();
    m_roverStates.reserve(roverCount);
    
    for (int i = 1; i <= roverCount; ++i) {
        const RoverProfile* profile = RoverProfiles::getRoverProfile(i);
        if (!profile) {
            Logger::error("Failed to get rover profile for rover {}", i);
            return false;
        }
        
        auto state = std::make_unique<RoverNetworkState>(i);
        
        // Create UDP sockets for this rover
        state->poseSocket = createUDPSocket(profile->posePort, false);
        state->lidarSocket = createUDPSocket(profile->lidarPort, false);
        state->telemSocket = createUDPSocket(profile->telemPort, false);
        state->cmdSocket = createUDPSocket(0, false);  // Client socket, no specific port
        
        if (state->poseSocket < 0 || state->lidarSocket < 0 || 
            state->telemSocket < 0 || state->cmdSocket < 0) {
            Logger::error("Failed to create UDP sockets for rover {}", i);
            return false;
        }
        
        Logger::debug("Created sockets for rover {}: pose={}, lidar={}, telem={}, cmd={}", 
                     i, state->poseSocket, state->lidarSocket, state->telemSocket, state->cmdSocket);
        
        m_roverStates.push_back(std::move(state));
    }
    
    // Set m_running before creating threads
    m_running = true;
    
    // Start receive threads - create them but don't wait for them to start
    m_receiveThreads.clear();
    m_receiveThreads.reserve(roverCount * 3);  // 3 threads per rover
    
    Logger::info("Creating receive threads for {} rovers...", roverCount);
    
    try {
        for (int i = 1; i <= roverCount; ++i) {
            Logger::debug("Creating threads for rover {}...", i);
            m_receiveThreads.emplace_back(&NetworkManager::poseReceiveThread, this, i);
            m_receiveThreads.emplace_back(&NetworkManager::lidarReceiveThread, this, i);
            m_receiveThreads.emplace_back(&NetworkManager::telemReceiveThread, this, i);
        }
        Logger::info("All {} threads created successfully", m_receiveThreads.size());
    } catch (const std::exception& e) {
        Logger::error("Failed to create threads: {}", e.what());
        m_running = false;
        return false;
    }
    
    Logger::info("NetworkManager initialized successfully with {} rovers", RoverProfiles::getRoverCount());
    return true;
}

void NetworkManager::shutdown() {
    if (!m_running) {
        return;
    }
    
    Logger::info("Shutting down NetworkManager...");
    
    // Stop threads
    m_running = false;
    
    // Wait for all threads to finish
    for (auto& thread : m_receiveThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_receiveThreads.clear();
    
    // Close sockets and cleanup
    for (auto& state : m_roverStates) {
        if (state->poseSocket >= 0) {
            close(state->poseSocket);
            state->poseSocket = -1;
        }
        if (state->lidarSocket >= 0) {
            close(state->lidarSocket);
            state->lidarSocket = -1;
        }
        if (state->telemSocket >= 0) {
            close(state->telemSocket);
            state->telemSocket = -1;
        }
        if (state->cmdSocket >= 0) {
            close(state->cmdSocket);
            state->cmdSocket = -1;
        }
    }
    
    m_roverStates.clear();
    
    Logger::info("NetworkManager shutdown complete");
}

void NetworkManager::update() {
    // Process command queue
    processCommandQueue();
    
    // Update rover online status
    auto currentTime = std::chrono::high_resolution_clock::now();
    for (auto& state : m_roverStates) {
        auto timeSinceLastPacket = std::chrono::duration<double>(currentTime - state->lastPacketTime).count() * 1000.0;
        bool wasOnline = state->online.load();
        bool isOnline = timeSinceLastPacket < NetworkConfig::ROVER_OFFLINE_TIMEOUT_MS;
        
        state->online = isOnline;
        
        if (wasOnline && !isOnline) {
            Logger::warn("Rover {} went offline (no packets for {:.1f}ms)", state->roverId, timeSinceLastPacket);
        } else if (!wasOnline && isOnline) {
            Logger::info("Rover {} came online", state->roverId);
        }
    }
    
    // Assemble and cleanup LiDAR data
    for (auto& state : m_roverStates) {
        assembleLidarScans(*state);
        cleanupExpiredLidarStates(*state);
    }
}

bool NetworkManager::sendButtonCommand(int roverId, uint8_t buttonMask) {
    if (!isValidRoverId(roverId)) {
        Logger::warn("Invalid rover ID for button command: {}", roverId);
        return false;
    }
    
    // Add command to queue for processing
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_commandQueue.push(std::make_pair(roverId, buttonMask));
    }
    
    Logger::debug("Queued button command for rover {}: {}", roverId, 
                 ButtonUtils::buttonStateToString(buttonMask));
    
    return true;
}

const PosePacket* NetworkManager::getLatestPose(int roverId) const {
    const RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(state->dataMutex);
    // Only return pose if it has valid data (timestamp > 0)
    if (state->latestPose.timestamp > 0.0) {
        return &state->latestPose;
    }
    return nullptr;
}

const VehicleTelem* NetworkManager::getLatestTelemetry(int roverId) const {
    const RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(state->dataMutex);
    return &state->latestTelem;
}

void NetworkManager::getCompletedLidarScans(int roverId, std::vector<CompletedLidarScan>& outScans) {
    outScans.clear();
    
    RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(state->dataMutex);
    
    // Get current time for timeout check
    auto now = std::chrono::high_resolution_clock::now();
    double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
    
    // Check for completed scans and move them to output
    auto it = state->lidarAssembly.begin();
    while (it != state->lidarAssembly.end()) {
        const auto& assemblyState = it->second;
        
        // Check if all chunks are received
        if (assemblyState.isComplete()) {
            // Assemble complete scan
            CompletedLidarScan scan;
            scan.timestamp = it->first;
            scan.roverId = roverId;
            scan.points = assemblyState.points;  // Copy all assembled points
            
            outScans.push_back(std::move(scan));
            
            // Remove from assembly map
            it = state->lidarAssembly.erase(it);
        } else if (assemblyState.isExpired(currentTime, NetworkConfig::PACKET_TIMEOUT_MS)) {
            // Timeout - discard partial scan
            Logger::warn("LiDAR scan timeout for rover {} timestamp {}, received {}/{} chunks",
                        roverId, it->first, assemblyState.receivedChunks, 
                        assemblyState.totalChunks);
            it = state->lidarAssembly.erase(it);
        } else {
            ++it;
        }
    }
}

NetworkStats NetworkManager::getNetworkStats(int roverId) const {
    const RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(state->dataMutex);
    return state->stats;
}

bool NetworkManager::isRoverOnline(int roverId) const {
    const RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        return false;
    }
    
    return state->online.load();
}

size_t NetworkManager::getTotalPointCount() const {
    return m_totalPointsReceived.load();
}

int NetworkManager::createUDPSocket(int port, bool blocking) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        Logger::error("Failed to create UDP socket: {}", strerror(errno));
        return -1;
    }
    
    // Set socket to non-blocking if requested
    if (!blocking) {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
            Logger::error("Failed to set socket non-blocking: {}", strerror(errno));
            close(sock);
            return -1;
        }
    }
    
    // Enable address reuse
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        Logger::warn("Failed to set SO_REUSEADDR: {}", strerror(errno));
    }
    
    // Bind socket if port is specified
    if (port > 0) {
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;  // Accept from any source
        addr.sin_port = htons(port);
        
        if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            Logger::error("Failed to bind socket to port {}: {}", port, strerror(errno));
            close(sock);
            return -1;
        }
        
        Logger::debug("Bound UDP socket to port {}", port);
    }
    
    return sock;
}

void NetworkManager::poseReceiveThread(int roverId) {
    // Small initial delay to ensure initialization is complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        Logger::error("Failed to get rover state for rover {} in pose thread", roverId);
        return;
    }
    
    Logger::debug("Pose receive thread started for rover {}", roverId);
    
    char buffer[sizeof(WirePosePacket)];
    sockaddr_in senderAddr;
    socklen_t senderLen;
    
    while (m_running) {
        senderLen = sizeof(senderAddr);
        ssize_t bytesReceived = recvfrom(state->poseSocket, buffer, sizeof(buffer), 0,
                                        (sockaddr*)&senderAddr, &senderLen);
        
        if (bytesReceived == sizeof(WirePosePacket)) {
            WirePosePacket wire;
            std::memcpy(&wire, buffer, sizeof(wire));
            PosePacket packet{wire.timestamp, wire.posX, wire.posY, wire.posZ,
                              wire.rotXdeg, wire.rotYdeg, wire.rotZdeg};
            processPosePacket(*state, packet);
        } else if (bytesReceived < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Actual error occurred
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
            }
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::debug("Pose receive thread ending for rover {}", roverId);
}

void NetworkManager::lidarReceiveThread(int roverId) {
    // Small initial delay to ensure initialization is complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        Logger::error("Failed to get rover state for rover {} in lidar thread", roverId);
        return;
    }
    
    Logger::debug("LiDAR receive thread started for rover {}", roverId);
    
    char buffer[sizeof(WireLidarPacket)];
    sockaddr_in senderAddr;
    socklen_t senderLen;
    
    while (m_running) {
        senderLen = sizeof(senderAddr);
        ssize_t bytesReceived = recvfrom(state->lidarSocket, buffer, sizeof(buffer), 0,
                                        (sockaddr*)&senderAddr, &senderLen);
        
        if (bytesReceived >= (ssize_t)sizeof(WireLidarPacketHeader)) {
            // Copy header into an aligned local struct
            WireLidarPacketHeader wireHeader;
            std::memcpy(&wireHeader, buffer, sizeof(WireLidarPacketHeader));

            uint32_t pointsCount = wireHeader.pointsInThisChunk;
            if (pointsCount > MAX_LIDAR_POINTS_PER_PACKET) {
                pointsCount = MAX_LIDAR_POINTS_PER_PACKET;
            }

            size_t expectedSize = sizeof(WireLidarPacketHeader) + pointsCount * sizeof(WireLidarPoint);
            if ((size_t)bytesReceived == expectedSize) {
                LidarPacket packet;
                packet.header.timestamp = wireHeader.timestamp;
                packet.header.chunkIndex = wireHeader.chunkIndex;
                packet.header.totalChunks = wireHeader.totalChunks;
                packet.header.pointsInThisChunk = pointsCount;

                // Copy points one by one to avoid unaligned access
                const char* pointData = buffer + sizeof(WireLidarPacketHeader);
                for (uint32_t i = 0; i < pointsCount; ++i) {
                    WireLidarPoint tmp;
                    std::memcpy(&tmp, pointData + i * sizeof(WireLidarPoint), sizeof(WireLidarPoint));
                    packet.points[i].x = tmp.x;
                    packet.points[i].y = tmp.y;
                    packet.points[i].z = tmp.z;
                }

                processLidarPacket(*state, packet);
            }
        } else if (bytesReceived < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
            }
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::debug("LiDAR receive thread ending for rover {}", roverId);
}

void NetworkManager::telemReceiveThread(int roverId) {
    // Small initial delay to ensure initialization is complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    RoverNetworkState* state = getRoverState(roverId);
    if (!state) {
        Logger::error("Failed to get rover state for rover {} in telemetry thread", roverId);
        return;
    }
    
    Logger::debug("Telemetry receive thread started for rover {}", roverId);
    
    char buffer[sizeof(WireVehicleTelem)];
    sockaddr_in senderAddr;
    socklen_t senderLen;
    
    while (m_running) {
        senderLen = sizeof(senderAddr);
        ssize_t bytesReceived = recvfrom(state->telemSocket, buffer, sizeof(buffer), 0,
                                        (sockaddr*)&senderAddr, &senderLen);
        
        if (bytesReceived == sizeof(WireVehicleTelem)) {
            WireVehicleTelem wire;
            std::memcpy(&wire, buffer, sizeof(wire));
            VehicleTelem packet{wire.timestamp, wire.buttonStates};
            processTelemPacket(*state, packet);
        } else if (bytesReceived < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
            }
        }
        
        // Small sleep to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::debug("Telemetry receive thread ending for rover {}", roverId);
}

void NetworkManager::processPosePacket(RoverNetworkState& state, const PosePacket& packet) {
    {
        std::lock_guard<std::mutex> lock(state.dataMutex);
        state.latestPose = packet;
    }
    
    state.lastPacketTime = std::chrono::high_resolution_clock::now();
    updateNetworkStats(state, PacketType::POSE);
    
    Logger::trace("Received pose for rover {}: pos=({:.2f}, {:.2f}, {:.2f}), rot=({:.1f}, {:.1f}, {:.1f})",
                 state.roverId, packet.posX, packet.posY, packet.posZ, 
                 packet.rotXdeg, packet.rotYdeg, packet.rotZdeg);
}

void NetworkManager::processLidarPacket(RoverNetworkState& state, const LidarPacket& packet) {
    {
        std::lock_guard<std::mutex> lock(state.dataMutex);
        
        double timestamp = packet.header.timestamp;
        auto& assembly = state.lidarAssembly[timestamp];
        
        // Initialize assembly state if first chunk
        if (assembly.timestamp == 0.0) {
            assembly.timestamp = timestamp;
            assembly.totalChunks = packet.header.totalChunks;
            assembly.firstChunkTime = getCurrentTime();
            assembly.points.reserve(assembly.totalChunks * MAX_LIDAR_POINTS_PER_PACKET);
        }
        
        // Check if this chunk was already received
        uint32_t chunkIndex = packet.header.chunkIndex;
        if (chunkIndex >= 256 || assembly.chunkReceived[chunkIndex]) {
            Logger::warn("Duplicate or invalid LiDAR chunk for rover {}: index {}", 
                        state.roverId, chunkIndex);
            return;
        }
        
        // Add points from this chunk
        for (uint32_t i = 0; i < packet.header.pointsInThisChunk; ++i) {
            assembly.points.push_back(packet.points[i]);
        }
        
        assembly.chunkReceived[chunkIndex] = true;
        assembly.receivedChunks++;
        
        m_totalPointsReceived += packet.header.pointsInThisChunk;
    }
    
    state.lastPacketTime = std::chrono::high_resolution_clock::now();
    updateNetworkStats(state, PacketType::LIDAR);
    
    Logger::trace("Received LiDAR chunk for rover {}: {}/{} chunks, {} points", 
                 state.roverId, packet.header.chunkIndex + 1, packet.header.totalChunks,
                 packet.header.pointsInThisChunk);
}

void NetworkManager::processTelemPacket(RoverNetworkState& state, const VehicleTelem& packet) {
    {
        std::lock_guard<std::mutex> lock(state.dataMutex);
        state.latestTelem = packet;
    }
    
    state.lastPacketTime = std::chrono::high_resolution_clock::now();
    updateNetworkStats(state, PacketType::TELEMETRY);
    
    Logger::trace("Received telemetry for rover {}: buttons={}", 
                 state.roverId, ButtonUtils::buttonStateToString(packet.buttonStates));
}

void NetworkManager::assembleLidarScans(RoverNetworkState& state) {
    std::lock_guard<std::mutex> lock(state.dataMutex);
    
    auto it = state.lidarAssembly.begin();
    while (it != state.lidarAssembly.end()) {
        auto& assembly = it->second;
        
        if (assembly.isComplete()) {
            // Complete scan - process it
            Logger::debug("Completed LiDAR scan for rover {} with {} points", 
                         state.roverId, assembly.points.size());
            
            // TODO: Add points to terrain buffer/point cloud storage
            
            it = state.lidarAssembly.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkManager::cleanupExpiredLidarStates(RoverNetworkState& state) {
    std::lock_guard<std::mutex> lock(state.dataMutex);
    
    double currentTime = getCurrentTime();
    auto it = state.lidarAssembly.begin();
    
    while (it != state.lidarAssembly.end()) {
        auto& assembly = it->second;
        
        if (assembly.isExpired(currentTime, NetworkConfig::PACKET_TIMEOUT_MS)) {
            Logger::warn("Expired incomplete LiDAR scan for rover {}: {}/{} chunks received", 
                        state.roverId, assembly.receivedChunks, assembly.totalChunks);
            it = state.lidarAssembly.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkManager::updateNetworkStats(RoverNetworkState& state, PacketType packetType) {
    std::lock_guard<std::mutex> lock(state.dataMutex);
    
    state.stats.lastPacketTime = getCurrentTime();
    state.stats.totalPackets++;
    
    // TODO: Implement more detailed statistics (packet loss, latency, etc.)
}

void NetworkManager::processCommandQueue() {
    std::lock_guard<std::mutex> lock(m_commandMutex);
    
    while (!m_commandQueue.empty()) {
        auto command = m_commandQueue.front();
        m_commandQueue.pop();
        
        int roverId = command.first;
        uint8_t buttonMask = command.second;
        
        const RoverProfile* profile = RoverProfiles::getRoverProfile(roverId);
        RoverNetworkState* state = getRoverState(roverId);
        
        if (!profile || !state) {
            Logger::error("Invalid rover for command: {}", roverId);
            continue;
        }
        
        // Send command
        ButtonCommand cmd;
        cmd.buttonMask = buttonMask;
        
        ssize_t bytesSent = sendUDP(state->cmdSocket, &cmd, sizeof(cmd), profile->cmdPort);
        
        if (bytesSent == sizeof(cmd)) {
            Logger::debug("Sent button command to rover {}: {}", roverId, 
                         ButtonUtils::buttonStateToString(buttonMask));
        } else {
            Logger::warn("Failed to send button command to rover {}: {}", roverId, strerror(errno));
        }
    }
}

ssize_t NetworkManager::sendUDP(int socket, const void* data, size_t size, int port) {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(NetworkConfig::LOOPBACK_ADDR);
    addr.sin_port = htons(port);
    
    return sendto(socket, data, size, 0, 
                  reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

double NetworkManager::getCurrentTime() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}

RoverNetworkState* NetworkManager::getRoverState(int roverId) {
    if (!isValidRoverId(roverId)) {
        return nullptr;
    }
    return m_roverStates[roverId - 1].get();
}

const RoverNetworkState* NetworkManager::getRoverState(int roverId) const {
    if (!isValidRoverId(roverId)) {
        return nullptr;
    }
    return m_roverStates[roverId - 1].get();
}

bool NetworkManager::isValidRoverId(int roverId) const {
    return roverId >= 1 && roverId <= static_cast<int>(m_roverStates.size());
}