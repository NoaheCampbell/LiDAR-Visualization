#ifndef ROVER_PROFILES_H
#define ROVER_PROFILES_H

// --------------------------------------------------------------------
// Rover's "profile" data:
// - dataFile: path to the .dat file
// - posePort: UDP port for pose (position/orientation) data
// - lidarPort: UDP port for LiDAR (point cloud) data
// - telemPort: UDP port for telemetry sending
// - cmdPort: UDP port for receiving button commands
// --------------------------------------------------------------------
struct RoverProfile {
    std::string dataFile;
    int posePort;
    int lidarPort;
    int telemPort;
    int cmdPort;
};


// List of rover profiles
static std::map<std::string, RoverProfile> g_roverProfiles = {
    { "1", { "data/rover1.dat", 9001, 10001, 11001, 8001} },
    { "2", { "data/rover2.dat", 9002, 10002, 11002, 8002} },
    { "3", { "data/rover3.dat", 9003, 10003, 11003, 8003} },
    { "4", { "data/rover4.dat", 9004, 10004, 11004, 8004} },
    { "5", { "data/rover5.dat", 9005, 10005, 11005, 8005} }
};

#endif // ROVER_PROFILES_H