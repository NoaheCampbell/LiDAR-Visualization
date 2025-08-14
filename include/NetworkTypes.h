#pragma once

#include <cstdint>

// Network packet structures for Multi-Rover LiDAR Visualization
// All structures are packed for UDP transmission compatibility

#pragma pack(push, 1)

/**
 * Pose data packet containing rover position and orientation
 */
struct PosePacket {
    double timestamp;       // Unix timestamp in seconds
    float posX, posY, posZ; // Position in meters (world coordinates)
    float rotXdeg, rotYdeg, rotZdeg; // Rotation in degrees (Euler angles)
};

/**
 * Header for LiDAR data packets
 * LiDAR data is transmitted in chunks due to UDP size limitations
 */
struct LidarPacketHeader {
    double timestamp;           // Unix timestamp in seconds
    uint32_t chunkIndex;        // Index of this chunk (0-based)
    uint32_t totalChunks;       // Total number of chunks for this scan
    uint32_t pointsInThisChunk; // Number of points in this specific chunk
};

/**
 * Individual LiDAR point in 3D space
 */
struct LidarPoint {
    float x, y, z; // Position in meters (rover-relative coordinates)
};

/**
 * Complete LiDAR packet containing header and point data
 * Maximum 100 points per packet as specified in PRD
 */
struct LidarPacket {
    LidarPacketHeader header;
    LidarPoint points[100]; // Fixed-size array for consistent packet size
};

/**
 * Vehicle telemetry packet containing button states
 */
struct VehicleTelem {
    double timestamp;       // Unix timestamp in seconds
    uint8_t buttonStates;   // Bitfield representing button states (8 buttons max)
};

/**
 * Command packet for sending instructions to rovers
 */
struct CommandPacket {
    double timestamp;       // Unix timestamp in seconds
    uint8_t commandType;    // Type of command being sent
    uint8_t commandData[7]; // Command-specific data (pad to 8 bytes total)
};

#pragma pack(pop)

// Network protocol constants
namespace NetworkConstants {
    constexpr size_t MAX_LIDAR_POINTS_PER_PACKET = 100;
    constexpr size_t MAX_UDP_PAYLOAD_SIZE = sizeof(LidarPacket);
    constexpr uint16_t PROTOCOL_VERSION = 1;
}