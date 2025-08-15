#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NetworkTypes.h"

struct StreamTimestamps {
    double lastPoseTs = 0.0;
    double lastLidarTs = 0.0;
    double lastTelemTs = 0.0;
};

class NetworkManager {
public:
    using PoseCallback = std::function<void(const std::string&, const PosePacket&)>;
    using LidarCallback = std::function<void(const std::string&, const LidarPacketHeader&, const LidarPoint*, size_t)>;
    using TelemCallback = std::function<void(const std::string&, const VehicleTelem&)>;

    NetworkManager();
    ~NetworkManager();

    void start(const std::map<std::string, int>& posePorts,
               const std::map<std::string, int>& lidarPorts,
               const std::map<std::string, int>& telemPorts);

    void stop();

    bool sendCommand(const std::string& roverId, uint8_t commandByte, int cmdPort);

    void setPoseCallback(PoseCallback cb) { poseCb = std::move(cb); }
    void setLidarCallback(LidarCallback cb) { lidarCb = std::move(cb); }
    void setTelemCallback(TelemCallback cb) { telemCb = std::move(cb); }

    StreamTimestamps getStreamTimestamps(const std::string& roverId) const;

private:
    void runReceiver(const std::string& roverId, int port, char streamType);

    std::atomic<bool> running {false};
    std::vector<std::thread> threads;
    PoseCallback poseCb;
    LidarCallback lidarCb;
    TelemCallback telemCb;

    mutable std::mutex tsMutex;
    std::map<std::string, StreamTimestamps> tsByRover;
};


