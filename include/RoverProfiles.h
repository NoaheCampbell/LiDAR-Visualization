#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>

/**
 * Rover profile management for Multi-Rover LiDAR Visualization
 * Handles port mapping and rover configuration as specified in PRD
 */

namespace RoverProfiles {
    
    /**
     * Network ports for a single rover
     */
    struct RoverPorts {
        uint16_t posePort;      // Port for pose data reception
        uint16_t lidarPort;     // Port for LiDAR data reception  
        uint16_t telemetryPort; // Port for telemetry data reception
        uint16_t commandPort;   // Port for command transmission
        
        RoverPorts() : posePort(0), lidarPort(0), telemetryPort(0), commandPort(0) {}
        
        RoverPorts(uint16_t pose, uint16_t lidar, uint16_t telemetry, uint16_t command)
            : posePort(pose), lidarPort(lidar), telemetryPort(telemetry), commandPort(command) {}
    };
    
    /**
     * Complete rover profile including network and display configuration
     */
    struct RoverProfile {
        uint8_t roverId;                    // Rover identifier (1-5)
        RoverPorts ports;                   // Network port configuration
        std::string displayName;            // Human-readable name
        float color[4];                     // RGBA color for visualization
        bool isActive;                      // Whether rover is currently active
        std::string ipAddress;              // Rover IP address (default: localhost)
        
        RoverProfile() : roverId(0), isActive(false), ipAddress("127.0.0.1") {
            color[0] = color[1] = color[2] = 1.0f; // White default
            color[3] = 1.0f;
        }
    };
    
    /**
     * Rover profile manager class
     */
    class RoverProfileManager {
    public:
        RoverProfileManager();
        ~RoverProfileManager() = default;
        
        // Profile access
        const RoverProfile* getRoverProfile(uint8_t roverId) const;
        RoverProfile* getRoverProfile(uint8_t roverId);
        
        // Port queries
        uint16_t getPosePort(uint8_t roverId) const;
        uint16_t getLidarPort(uint8_t roverId) const;
        uint16_t getTelemetryPort(uint8_t roverId) const;
        uint16_t getCommandPort(uint8_t roverId) const;
        
        // Rover management
        bool isValidRoverId(uint8_t roverId) const;
        void setRoverActive(uint8_t roverId, bool active);
        bool isRoverActive(uint8_t roverId) const;
        
        // Configuration
        void setRoverIPAddress(uint8_t roverId, const std::string& ipAddress);
        std::string getRoverIPAddress(uint8_t roverId) const;
        
        // Utility functions
        std::array<uint8_t, 5> getActiveRovers() const;
        size_t getActiveRoverCount() const;
        void enableAllRovers();
        void disableAllRovers();
        
        // Port validation
        bool isPortInUse(uint16_t port) const;
        std::vector<uint16_t> getAllUsedPorts() const;
        
    private:
        std::unordered_map<uint8_t, RoverProfile> profiles_;
        
        void initializeDefaultProfiles();
        void validateRoverId(uint8_t roverId) const;
    };
    
    // Global instance access
    RoverProfileManager& getInstance();
    
    // Convenience functions for direct access
    const RoverProfile* getRover(uint8_t roverId);
    uint16_t getPosePort(uint8_t roverId);
    uint16_t getLidarPort(uint8_t roverId);
    uint16_t getTelemetryPort(uint8_t roverId);
    uint16_t getCommandPort(uint8_t roverId);
    bool isValidRover(uint8_t roverId);
    
} // namespace RoverProfiles