#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>

#include "NetworkTypes.h"

struct CompletedScan {
    std::string roverId;
    double timestamp;
    std::vector<LidarPoint> points;
};

class DataAssembler {
public:
    void addChunk(const std::string& roverId, const LidarPacketHeader& hdr, const LidarPoint* pts, size_t count);

    // Move completed scans out
    std::vector<CompletedScan> retrieveCompleted();

    void setMaxPoints(size_t maxPoints) { maxPointsGlobal = maxPoints; }
    void setStoreGlobalPoints(bool enable) { storeGlobalPoints = enable; }

    // Global terrain buffer access
    const std::vector<LidarPoint>& getGlobalTerrain() const { return globalTerrain; }

    // Maintenance, drop old partials and optionally fade
    void maintenance(double nowSeconds);

private:
    struct PartialKey {
        std::string roverId;
        double timestamp;
        bool operator==(const PartialKey& other) const {
            return roverId == other.roverId && timestamp == other.timestamp;
        }
    };
    struct PartialKeyHash {
        std::size_t operator()(const PartialKey& k) const {
            return std::hash<std::string>()(k.roverId) ^ std::hash<long long>()(static_cast<long long>(k.timestamp * 1e6));
        }
    };

    struct PartialScan {
        double firstArrivalTs = 0.0; // wall time
        uint32_t totalChunks = 0;
        std::vector<bool> received;
        std::vector<LidarPoint> points; // will accumulate
    };

    mutable std::mutex mutex;
    std::unordered_map<PartialKey, PartialScan, PartialKeyHash> partials;
    std::deque<CompletedScan> completed;

    std::vector<LidarPoint> globalTerrain;
    size_t maxPointsGlobal = 2'000'000; // auto-tune later
    bool storeGlobalPoints = false;
};


