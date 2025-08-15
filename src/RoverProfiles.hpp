#pragma once

#include <map>
#include <string>

struct RoverProfile {
    std::string id;
    int posePort;
    int lidarPort;
    int telemPort;
    int cmdPort;
};

static inline std::map<std::string, RoverProfile> getDefaultProfiles() {
    return {
        {"1", {"1", 9001, 10001, 11001, 8001}},
        {"2", {"2", 9002, 10002, 11002, 8002}},
        {"3", {"3", 9003, 10003, 11003, 8003}},
        {"4", {"4", 9004, 10004, 11004, 8004}},
        {"5", {"5", 9005, 10005, 11005, 8005}},
    };
}


