#include "RoverProfiles.h"
#include "Constants.h"
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace RoverProfiles {
    
    RoverProfileManager::RoverProfileManager() {
        initializeDefaultProfiles();
    }
    
    void RoverProfileManager::initializeDefaultProfiles() {
        // Initialize rover profiles based on PRD port mapping specification
        const std::array<std::string, 5> roverNames = {
            "Rover Alpha", "Rover Beta", "Rover Gamma", "Rover Delta", "Rover Echo"
        };
        
        for (uint8_t i = 1; i <= 5; ++i) {
            RoverProfile profile;
            profile.roverId = i;
            profile.displayName = roverNames[i - 1];
            
            // Port mapping as specified in PRD
            profile.ports.posePort = 9000 + i;      // 9001-9005
            profile.ports.lidarPort = 10000 + i;    // 10001-10005
            profile.ports.telemetryPort = 11000 + i; // 11001-11005
            profile.ports.commandPort = 8000 + i;   // 8001-8005
            
            // Default IP address (localhost for development)
            profile.ipAddress = "127.0.0.1";
            
            // Assign colors from Constants
            if (i <= 5) {
                for (int j = 0; j < 4; ++j) {
                    profile.color[j] = Constants::Colors::ROVER_COLORS[i - 1][j];
                }
            }
            
            // All rovers start as active by default
            profile.isActive = true;
            
            profiles_[i] = profile;
        }
    }
    
    void RoverProfileManager::validateRoverId(uint8_t roverId) const {
        if (roverId < Constants::Rover::MIN_ROVER_ID || roverId > Constants::Rover::MAX_ROVER_ID) {
            throw std::invalid_argument("Invalid rover ID: " + std::to_string(roverId));
        }
    }
    
    const RoverProfile* RoverProfileManager::getRoverProfile(uint8_t roverId) const {
        validateRoverId(roverId);
        auto it = profiles_.find(roverId);
        return (it != profiles_.end()) ? &it->second : nullptr;
    }
    
    RoverProfile* RoverProfileManager::getRoverProfile(uint8_t roverId) {
        validateRoverId(roverId);
        auto it = profiles_.find(roverId);
        return (it != profiles_.end()) ? &it->second : nullptr;
    }
    
    uint16_t RoverProfileManager::getPosePort(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->ports.posePort : 0;
    }
    
    uint16_t RoverProfileManager::getLidarPort(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->ports.lidarPort : 0;
    }
    
    uint16_t RoverProfileManager::getTelemetryPort(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->ports.telemetryPort : 0;
    }
    
    uint16_t RoverProfileManager::getCommandPort(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->ports.commandPort : 0;
    }
    
    bool RoverProfileManager::isValidRoverId(uint8_t roverId) const {
        return (roverId >= Constants::Rover::MIN_ROVER_ID && 
                roverId <= Constants::Rover::MAX_ROVER_ID &&
                profiles_.find(roverId) != profiles_.end());
    }
    
    void RoverProfileManager::setRoverActive(uint8_t roverId, bool active) {
        RoverProfile* profile = getRoverProfile(roverId);
        if (profile) {
            profile->isActive = active;
        }
    }
    
    bool RoverProfileManager::isRoverActive(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->isActive : false;
    }
    
    void RoverProfileManager::setRoverIPAddress(uint8_t roverId, const std::string& ipAddress) {
        RoverProfile* profile = getRoverProfile(roverId);
        if (profile) {
            profile->ipAddress = ipAddress;
        }
    }
    
    std::string RoverProfileManager::getRoverIPAddress(uint8_t roverId) const {
        const RoverProfile* profile = getRoverProfile(roverId);
        return profile ? profile->ipAddress : "";
    }
    
    std::array<uint8_t, 5> RoverProfileManager::getActiveRovers() const {
        std::array<uint8_t, 5> activeRovers{};
        size_t count = 0;
        
        for (const auto& pair : profiles_) {
            if (pair.second.isActive && count < 5) {
                activeRovers[count++] = pair.first;
            }
        }
        
        return activeRovers;
    }
    
    size_t RoverProfileManager::getActiveRoverCount() const {
        return std::count_if(profiles_.begin(), profiles_.end(),
                           [](const auto& pair) { return pair.second.isActive; });
    }
    
    void RoverProfileManager::enableAllRovers() {
        for (auto& pair : profiles_) {
            pair.second.isActive = true;
        }
    }
    
    void RoverProfileManager::disableAllRovers() {
        for (auto& pair : profiles_) {
            pair.second.isActive = false;
        }
    }
    
    bool RoverProfileManager::isPortInUse(uint16_t port) const {
        for (const auto& pair : profiles_) {
            const RoverPorts& ports = pair.second.ports;
            if (ports.posePort == port || ports.lidarPort == port ||
                ports.telemetryPort == port || ports.commandPort == port) {
                return true;
            }
        }
        return false;
    }
    
    std::vector<uint16_t> RoverProfileManager::getAllUsedPorts() const {
        std::vector<uint16_t> usedPorts;
        usedPorts.reserve(profiles_.size() * 4); // 4 ports per rover
        
        for (const auto& pair : profiles_) {
            const RoverPorts& ports = pair.second.ports;
            usedPorts.push_back(ports.posePort);
            usedPorts.push_back(ports.lidarPort);
            usedPorts.push_back(ports.telemetryPort);
            usedPorts.push_back(ports.commandPort);
        }
        
        return usedPorts;
    }
    
    // Global instance management
    RoverProfileManager& getInstance() {
        static RoverProfileManager instance;
        return instance;
    }
    
    // Convenience functions for direct access
    const RoverProfile* getRover(uint8_t roverId) {
        return getInstance().getRoverProfile(roverId);
    }
    
    uint16_t getPosePort(uint8_t roverId) {
        return getInstance().getPosePort(roverId);
    }
    
    uint16_t getLidarPort(uint8_t roverId) {
        return getInstance().getLidarPort(roverId);
    }
    
    uint16_t getTelemetryPort(uint8_t roverId) {
        return getInstance().getTelemetryPort(roverId);
    }
    
    uint16_t getCommandPort(uint8_t roverId) {
        return getInstance().getCommandPort(roverId);
    }
    
    bool isValidRover(uint8_t roverId) {
        return getInstance().isValidRoverId(roverId);
    }
    
} // namespace RoverProfiles