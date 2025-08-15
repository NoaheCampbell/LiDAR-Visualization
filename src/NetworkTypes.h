#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct PosePacket {
    double timestamp;
    float posX;
    float posY;
    float posZ;
    float rotXdeg;
    float rotYdeg;
    float rotZdeg;
};

static const size_t MAX_LIDAR_POINTS_PER_PACKET = 100;

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

struct VehicleTelem {
    double timestamp;
    uint8_t buttonStates; // bits 0..3
};
#pragma pack(pop)


