#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <map>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <random>

#include "rover_profiles.h"

#define LOOPBACK_ADDR "127.0.0.1"

// --------------------------------------------------------------------
// Simple function to create a UDP socket (IPv4, non-blocking).
// --------------------------------------------------------------------
int createUDPSocket()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Error: cannot create UDP socket.\n";
        std::exit(EXIT_FAILURE);
    }
    return sock;
}

// --------------------------------------------------------------------
// Sends a buffer via UDP to the specified port on localhost (127.0.0.1).
// Returns number of bytes sent, or -1 on error.
// --------------------------------------------------------------------
ssize_t sendUDP(int sock, const void* data, size_t dataSize, int port)
{
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(LOOPBACK_ADDR);

    return sendto(sock, data, dataSize, 0,
                  reinterpret_cast<sockaddr*>(&addr),
                  sizeof(addr));
}

// --------------------------------------------------------------------
// Helper: splits a string by delimiter (returns vector of tokens).
// --------------------------------------------------------------------
std::vector<std::string> splitString(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos) {
            tokens.push_back(s.substr(start));
            break;
        }
        tokens.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return tokens;
}

// --------------------------------------------------------------------
// Pose packet structure.
// --------------------------------------------------------------------
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
#pragma pack(pop)

// --------------------------------------------------------------------
// LiDAR packet structure
// --------------------------------------------------------------------
static const size_t MAX_LIDAR_POINTS_PER_PACKET = 100;

#pragma pack(push, 1)
struct LidarPacketHeader {
    double timestamp;
    uint32_t chunkIndex;
    uint32_t totalChunks;
    uint32_t pointsInThisChunk;
};

// Each point is 3 floats
struct LidarPoint {
    float x;
    float y;
    float z;
};

struct LidarPacket {
    LidarPacketHeader header;
    LidarPoint points[MAX_LIDAR_POINTS_PER_PACKET];
};

struct VehicleTelem {
    double timestamp;
    uint8_t buttonStates;  // bits 0..3 represent buttons 0..3
};
#pragma pack(pop)

// --------------------------------------------------------------------
// parseLine: Given a single line from the data file, parse the pose
// and the list of LiDAR points.
//
// Format: posX,posY,posZ,rotX,rotY,rotZ; x1,y1,z1; x2,y2,z2; ...
// --------------------------------------------------------------------
bool parseLine(const std::string& line,
               float& posX, float& posY, float& posZ,
               float& rotX, float& rotY, float& rotZ,
               std::vector<LidarPoint>& cloud)
{
    // 1) Split at the first semicolon to separate "pose" from "points"
    size_t semicolonPos = line.find(';');
    if (semicolonPos == std::string::npos) {
        std::cerr << "Invalid line (no semicolon): " << line << std::endl;
        return false;
    }

    std::string posePart = line.substr(0, semicolonPos);
    std::string pointsPart = line.substr(semicolonPos + 1);

    // 2) Parse the pose part (posX,posY,posZ,rotX,rotY,rotZ)
    {
        std::vector<std::string> poseTokens = splitString(posePart, ',');
        if (poseTokens.size() < 6) {
            std::cerr << "Invalid pose part: " << posePart << std::endl;
            return false;
        }
        posX = std::stof(poseTokens[0]);
        posY = std::stof(poseTokens[1]);
        posZ = std::stof(poseTokens[2]);
        rotX = std::stof(poseTokens[3]);
        rotY = std::stof(poseTokens[4]);
        rotZ = std::stof(poseTokens[5]);
    }

    // 3) Parse the rest of the line for points, each separated by ';'
    std::vector<std::string> pointTokens = splitString(pointsPart, ';');
    cloud.clear();
    cloud.reserve(pointTokens.size());

    for (const auto& pt : pointTokens) {
        // Each pt is "x,y,z"
        std::vector<std::string> coords = splitString(pt, ',');
        if (coords.size() < 3) {
            // Possibly last empty token?
            continue;
        }
        LidarPoint p;
        p.x = std::stof(coords[0]);
        p.y = std::stof(coords[1]);
        p.z = std::stof(coords[2]);
        cloud.push_back(p);
    }

    return true;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ROVER_ID>\n";
        return 1;
    }
    std::string roverID = argv[1];

    // Look up the rover's profile
    auto it = g_roverProfiles.find(roverID);
    if (it == g_roverProfiles.end()) {
        std::cerr << "Error: No profile found for rover ID: " << roverID << "\n";
        return 1;
    }
    RoverProfile profile = it->second;

    bool noNoise = false;
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--no-noise") {
            noNoise = true;
        }
    }

    static std::default_random_engine rng(std::random_device{}());
    std::normal_distribution<float> dist(0.0f, 0.5f);

    // Open the data file
    std::ifstream fin(profile.dataFile);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open data file: " << profile.dataFile << "\n";
        return 1;
    }

    // Create UDP sockets for sending pose & LiDAR
    int udpSockPose  = createUDPSocket();
    int udpSockLidar = createUDPSocket();
    int udpSockTelem = createUDPSocket();

    // Listen for button commands
    int cmdSock = createUDPSocket();

    // Bind cmdSock to cmdPort on localhost
    int cmdPort = profile.cmdPort;
    {
        sockaddr_in cmdAddr;
        std::memset(&cmdAddr, 0, sizeof(cmdAddr));
        cmdAddr.sin_family = AF_INET;
        cmdAddr.sin_port   = htons(cmdPort);
        cmdAddr.sin_addr.s_addr = inet_addr(LOOPBACK_ADDR);

        if (bind(cmdSock, reinterpret_cast<sockaddr*>(&cmdAddr), sizeof(cmdAddr)) < 0) {
            std::cerr << "Error: cannot bind command socket on port " << cmdPort << "\n";
            return 1;
        }
    }

    uint8_t buttonStates = 0;

    const double freqHz = 10.0;
    const std::chrono::milliseconds loopDelayMs((int)(1000.0 / freqHz));

    auto startTime = std::chrono::steady_clock::now();

    // Read line by line
    std::string line;
    while (true) {
        if (!std::getline(fin, line)) {
            // End of file
            break;
        }
        if (line.empty()) {
            continue;
        }

        float posX=0, posY=0, posZ=0;
        float rotX=0, rotY=0, rotZ=0;
        std::vector<LidarPoint> cloud;

        if (!parseLine(line, posX, posY, posZ, rotX, rotY, rotZ, cloud)) {
            std::cerr << "Failed to parse line.\n";
            continue;
        }

        // Inject noise if !noNoise:
        if (!noNoise) {
            posX += dist(rng);
            posY += dist(rng);
            posZ += dist(rng);
            rotX += dist(rng);
            rotY += dist(rng);
            rotZ += dist(rng);
            for (auto& p : cloud) {
                p.x += dist(rng);
                p.y += dist(rng);
                p.z += dist(rng);
            }
        }

        // Create a timestamp (seconds since start)
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - startTime;
        double timestamp = elapsed.count();

        // 1) Build the PosePacket
        PosePacket posePacket;
        posePacket.timestamp = timestamp;
        posePacket.posX = posX;
        posePacket.posY = posY;
        posePacket.posZ = posZ;
        posePacket.rotXdeg = rotX;
        posePacket.rotYdeg = rotY;
        posePacket.rotZdeg = rotZ;

        // 2) Send the pose over the pose port
        sendUDP(udpSockPose, &posePacket, sizeof(posePacket), profile.posePort);

        // 3) Break the LiDAR cloud into chunks of size <= MAX_LIDAR_POINTS_PER_PACKET
        size_t totalPoints = cloud.size();
        size_t totalChunks = (totalPoints + MAX_LIDAR_POINTS_PER_PACKET - 1) / MAX_LIDAR_POINTS_PER_PACKET;

        // For each chunk, build a LidarPacket
        for (size_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex) {
            LidarPacket packet;
            std::memset(&packet, 0, sizeof(packet));

            packet.header.timestamp = timestamp;
            packet.header.chunkIndex = static_cast<uint32_t>(chunkIndex);
            packet.header.totalChunks = static_cast<uint32_t>(totalChunks);

            // Fill points in this chunk
            size_t startIdx = chunkIndex * MAX_LIDAR_POINTS_PER_PACKET;
            size_t endIdx = std::min(startIdx + MAX_LIDAR_POINTS_PER_PACKET, totalPoints);
            size_t numPts = endIdx - startIdx;
            packet.header.pointsInThisChunk = static_cast<uint32_t>(numPts);

            for (size_t i = 0; i < numPts; ++i) {
                packet.points[i] = cloud[startIdx + i];
            }

            // Send the packet over the LiDAR port
            size_t packetSize = sizeof(LidarPacketHeader) + (numPts * sizeof(LidarPoint));
            sendUDP(udpSockLidar, &packet, packetSize, profile.lidarPort);
        }

        // 6) Check for incoming button command on cmdSock (non-blocking)
        uint8_t cmdByte = 0;
        ssize_t n = recv(cmdSock, &cmdByte, 1, MSG_DONTWAIT);
        if (n == 1) {
            // Update buttonStates with newly received bits
            buttonStates = cmdByte;
        }
        VehicleTelem telem;
        telem.timestamp    = timestamp;
        telem.buttonStates = buttonStates;

        int telemPort = profile.telemPort;
        sendUDP(udpSockTelem, &telem, sizeof(telem), telemPort);

        // 4) Sleep the remainder of the 10 Hz cycle
        std::this_thread::sleep_for(loopDelayMs);
    }

    // Clean up
    close(udpSockPose);
    close(udpSockLidar);
    fin.close();

    std::cout << "Finished streaming rover " << roverID << " data.\n";
    return 0;
}
