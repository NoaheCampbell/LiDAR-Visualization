#ifndef ROVER_PROFILES_H
#define ROVER_PROFILES_H

#include <string>
#include <map>
#include <mutex>

/**
 * Rover configuration profile containing network and data file information
 */
struct RoverProfile {
    std::string dataFile;
    int posePort;
    int lidarPort;
    int telemPort;
    int cmdPort;
};

/**
 * Thread-safe wrapper for rover profile management
 * Provides centralized access to rover configurations loaded from rover_profiles.h
 */
class RoverProfiles {
public:
    /**
     * Initialize the rover profiles system
     * Must be called before any other methods
     * @return true if initialization successful
     */
    static bool initialize();
    
    /**
     * Get the total number of configured rovers
     * @return number of rovers
     */
    static int getRoverCount();
    
    /**
     * Get rover profile by rover ID
     * @param roverId rover identifier (1-based)
     * @return pointer to profile or nullptr if not found
     */
    static const RoverProfile* getRoverProfile(int roverId);
    
    /**
     * Validate that all rover profiles have valid configurations
     * @return true if all profiles are valid
     */
    static bool validateProfiles();
    
    /**
     * Check if a rover ID is valid
     * @param roverId rover identifier to check
     * @return true if rover ID exists
     */
    static bool isValidRoverId(int roverId);
    
    /**
     * Check if a port is already in use by any rover
     * @param port port number to check
     * @return true if port is in use
     */
    static bool isPortInUse(int port);
    
    /**
     * Get all configured rover IDs
     * @return vector of rover IDs
     */
    static std::vector<int> getAllRoverIds();

private:
    static std::map<int, RoverProfile> s_profiles;
    static std::mutex s_mutex;
    static bool s_initialized;
    
    // Load profiles from the emulator configuration
    static void loadProfiles();
    
    // Validate a single profile
    static bool validateProfile(const RoverProfile& profile);
};

#endif // ROVER_PROFILES_H