#include "DataAssembler.hpp"

#include <algorithm>
#include <chrono>

static double nowSeconds() {
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now = clock::now();
    std::chrono::duration<double> d = now - start;
    return d.count();
}

void DataAssembler::addChunk(const std::string& roverId, const LidarPacketHeader& hdr, const LidarPoint* pts, size_t count) {
    std::lock_guard<std::mutex> lk(mutex);
    PartialKey key{roverId, hdr.timestamp};
    auto& partial = partials[key];
    if (partial.received.empty()) {
        partial.firstArrivalTs = nowSeconds();
        partial.totalChunks = hdr.totalChunks;
        partial.received.assign(hdr.totalChunks, false);
        partial.points.reserve(hdr.totalChunks * 80); // heuristic
    }
    if (hdr.chunkIndex < partial.received.size() && !partial.received[hdr.chunkIndex]) {
        partial.received[hdr.chunkIndex] = true;
        partial.points.insert(partial.points.end(), pts, pts + count);
    }
    // Check completion
    bool all = true;
    for (bool r : partial.received) {
        if (!r) { all = false; break; }
    }
    if (all) {
        CompletedScan scan;
        scan.roverId = roverId;
        scan.timestamp = hdr.timestamp;
        scan.points = std::move(partial.points);
        partials.erase(key);
        completed.push_back(std::move(scan));
    }
}

std::vector<CompletedScan> DataAssembler::retrieveCompleted() {
    std::lock_guard<std::mutex> lk(mutex);
    std::vector<CompletedScan> out;
    out.insert(out.end(), completed.begin(), completed.end());
    completed.clear();
    return out;
}

void DataAssembler::maintenance(double /*nowSec*/) {
    std::lock_guard<std::mutex> lk(mutex);
    // Drop stale partials (>200ms from first arrival)
    double t = nowSeconds();
    for (auto it = partials.begin(); it != partials.end();) {
        if (t - it->second.firstArrivalTs > 0.2) {
            it = partials.erase(it);
        } else {
            ++it;
        }
    }

    // Optionally mirror completed points into a global point buffer
    if (storeGlobalPoints) {
        auto tmp = completed; // copy so retrieveCompleted still returns them
        for (const auto& sc : tmp) {
            globalTerrain.insert(globalTerrain.end(), sc.points.begin(), sc.points.end());
        }
        // Enforce cap if configured
        if (maxPointsGlobal > 0 && globalTerrain.size() > maxPointsGlobal) {
            size_t drop = globalTerrain.size() - maxPointsGlobal;
            globalTerrain.erase(globalTerrain.begin(), globalTerrain.begin() + static_cast<long>(drop));
        }
    }
}


