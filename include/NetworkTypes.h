#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <cstdint>
#include <vector>
#include <algorithm>

// Maximum number of LiDAR points that can fit in a single UDP packet
static constexpr size_t MAX_LIDAR_POINTS_PER_PACKET = 100;

// Define packed WIRE structs used for network I/O only
#pragma pack(push, 1)
struct WirePosePacket {
    double timestamp;
    float posX;
    float posY;
    float posZ;
    float rotXdeg;
    float rotYdeg;
    float rotZdeg;
};

struct WireLidarPacketHeader {
    double timestamp;
    uint32_t chunkIndex;
    uint32_t totalChunks;
    uint32_t pointsInThisChunk;
};

struct WireLidarPoint {
    float x;
    float y;
    float z;
};

struct WireLidarPacket {
    WireLidarPacketHeader header;
    WireLidarPoint points[MAX_LIDAR_POINTS_PER_PACKET];
};

struct WireVehicleTelem {
    double timestamp;
    uint8_t buttonStates;
};

struct WireButtonCommand {
    uint8_t buttonMask;
};
#pragma pack(pop)

// Native (aligned) structs used throughout the application
struct PosePacket {
    double timestamp;
    float posX;
    float posY;
    float posZ;
    float rotXdeg;
    float rotYdeg;
    float rotZdeg;
};

struct LidarPacketHeader {
    double timestamp;
    uint32_t chunkIndex;
    uint32_t totalChunks;
    uint32_t pointsInThisChunk;
};

struct LidarPoint {
    float x;
    float y;
    float z;
};

struct LidarPacket {
    LidarPacketHeader header;
    LidarPoint points[MAX_LIDAR_POINTS_PER_PACKET];
};

struct VehicleTelem {
    double timestamp;
    uint8_t buttonStates;
};

struct ButtonCommand {
    uint8_t buttonMask;
};

/**
 * Network packet types for identification
 */
enum class PacketType {
    POSE,
    LIDAR,
    TELEMETRY,
    COMMAND
};

/**
 * Network statistics structure for diagnostics
 */
struct NetworkStats {
    double lastPacketTime;      // Last packet timestamp
    uint64_t totalPackets;      // Total packets received
    uint64_t lostPackets;       // Estimated lost packets
    double packetLossRate;      // Packet loss rate (0.0 - 1.0)
    double latency;             // Average latency in milliseconds
    uint64_t bytesReceived;     // Total bytes received
};

/**
 * LiDAR assembly state for tracking incomplete scans
 */
struct LidarAssemblyState {
    double timestamp;                           // Scan timestamp
    uint32_t totalChunks;                      // Expected total chunks
    uint32_t receivedChunks;                   // Number of chunks received
    bool chunkReceived[256];                   // Track which chunks received (max 256)
    std::vector<LidarPoint> points;            // Accumulated points
    double firstChunkTime;                     // Time first chunk was received
    
    LidarAssemblyState() : timestamp(0), totalChunks(0), receivedChunks(0), 
                          firstChunkTime(0) {
        std::fill(chunkReceived, chunkReceived + 256, false);
    }
    
    bool isComplete() const {
        return receivedChunks == totalChunks && totalChunks > 0;
    }
    
    bool isExpired(double currentTime, double timeoutMs = 200.0) const {
        return (currentTime - firstChunkTime) > (timeoutMs / 1000.0);
    }
};

#endif // NETWORK_TYPES_H