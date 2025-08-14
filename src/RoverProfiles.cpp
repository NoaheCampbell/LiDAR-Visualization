#include "RoverProfiles.h"
#include <map>
#include <string>
#include <algorithm>
#include <iostream>

// Need to include the required headers before including rover_profiles.h
// and declare the namespace to avoid conflicts
namespace {
    #include <map>
    #include <string>
    
    // Manually define the rover profile map since the emulator header is incomplete
    struct EmulatorRoverProfile {
        std::string dataFile;
        int posePort;
        int lidarPort;
        int telemPort;
        int cmdPort;
    };
    
    static std::map<std::string, EmulatorRoverProfile> g_emulator_rover_profiles = {
        { "1", { "data/rover1.dat", 9001, 10001, 11001, 8001} },
        { "2", { "data/rover2.dat", 9002, 10002, 11002, 8002} },
        { "3", { "data/rover3.dat", 9003, 10003, 11003, 8003} },
        { "4", { "data/rover4.dat", 9004, 10004, 11004, 8004} },
        { "5", { "data/rover5.dat", 9005, 10005, 11005, 8005} }
    };
}

// Static member definitions
std::map<int, RoverProfile> RoverProfiles::s_profiles;
std::mutex RoverProfiles::s_mutex;
bool RoverProfiles::s_initialized = false;

bool RoverProfiles::initialize() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized) {
        return true;
    }
    
    loadProfiles();
    
    if (s_profiles.empty()) {
        std::cerr << "Error: No rover profiles loaded" << std::endl;
        return false;
    }
    
    if (!validateProfiles()) {
        std::cerr << "Error: Invalid rover profile configuration" << std::endl;
        return false;
    }
    
    s_initialized = true;
    return true;
}

int RoverProfiles::getRoverCount() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return static_cast<int>(s_profiles.size());
}

const RoverProfile* RoverProfiles::getRoverProfile(int roverId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = s_profiles.find(roverId);
    if (it != s_profiles.end()) {
        return &it->second;
    }
    return nullptr;
}

bool RoverProfiles::validateProfiles() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_profiles.empty()) {
        return false;
    }
    
    // Check each profile individually
    for (const auto& pair : s_profiles) {
        if (!validateProfile(pair.second)) {
            std::cerr << "Invalid profile for rover " << pair.first << std::endl;
            return false;
        }
    }
    
    // Check for port conflicts
    std::vector<int> allPorts;
    for (const auto& pair : s_profiles) {
        const RoverProfile& profile = pair.second;
        allPorts.push_back(profile.posePort);
        allPorts.push_back(profile.lidarPort);
        allPorts.push_back(profile.telemPort);
        allPorts.push_back(profile.cmdPort);
    }
    
    std::sort(allPorts.begin(), allPorts.end());
    auto it = std::adjacent_find(allPorts.begin(), allPorts.end());
    if (it != allPorts.end()) {
        std::cerr << "Error: Duplicate port " << *it << " found in rover profiles" << std::endl;
        return false;
    }
    
    return true;
}

bool RoverProfiles::isValidRoverId(int roverId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_profiles.find(roverId) != s_profiles.end();
}

bool RoverProfiles::isPortInUse(int port) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    for (const auto& pair : s_profiles) {
        const RoverProfile& profile = pair.second;
        if (profile.posePort == port || profile.lidarPort == port || 
            profile.telemPort == port || profile.cmdPort == port) {
            return true;
        }
    }
    return false;
}

std::vector<int> RoverProfiles::getAllRoverIds() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    std::vector<int> ids;
    ids.reserve(s_profiles.size());
    
    for (const auto& pair : s_profiles) {
        ids.push_back(pair.first);
    }
    
    std::sort(ids.begin(), ids.end());
    return ids;
}

void RoverProfiles::loadProfiles() {
    // Convert the local rover profile map to our internal format
    for (const auto& pair : g_emulator_rover_profiles) {
        int roverId = std::stoi(pair.first);  // Convert string key to int
        const auto& emulatorProfile = pair.second;
        
        RoverProfile profile;
        profile.dataFile = emulatorProfile.dataFile;
        profile.posePort = emulatorProfile.posePort;
        profile.lidarPort = emulatorProfile.lidarPort;
        profile.telemPort = emulatorProfile.telemPort;
        profile.cmdPort = emulatorProfile.cmdPort;
        
        s_profiles[roverId] = profile;
    }
}

bool RoverProfiles::validateProfile(const RoverProfile& profile) {
    // Check that data file path is not empty
    if (profile.dataFile.empty()) {
        return false;
    }
    
    // Check that all ports are in valid range (1024-65535)
    const int MIN_PORT = 1024;
    const int MAX_PORT = 65535;
    
    if (profile.posePort < MIN_PORT || profile.posePort > MAX_PORT ||
        profile.lidarPort < MIN_PORT || profile.lidarPort > MAX_PORT ||
        profile.telemPort < MIN_PORT || profile.telemPort > MAX_PORT ||
        profile.cmdPort < MIN_PORT || profile.cmdPort > MAX_PORT) {
        return false;
    }
    
    // Check that all ports within a profile are unique
    std::vector<int> ports = {profile.posePort, profile.lidarPort, 
                              profile.telemPort, profile.cmdPort};
    std::sort(ports.begin(), ports.end());
    auto it = std::adjacent_find(ports.begin(), ports.end());
    if (it != ports.end()) {
        return false;  // Duplicate port within this profile
    }
    
    return true;
}