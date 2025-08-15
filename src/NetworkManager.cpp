#include "NetworkManager.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>

namespace {
int createUdpSocketBind(int port) {
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::perror("socket");
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(sock);
        return -1;
    }
    return sock;
}
}

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager() { stop(); }

void NetworkManager::start(const std::map<std::string, int>& posePorts,
                           const std::map<std::string, int>& lidarPorts,
                           const std::map<std::string, int>& telemPorts) {
    if (running.exchange(true)) return;

    auto spawn = [&](const std::map<std::string, int>& mp, char type) {
        for (const auto& [id, port] : mp) {
            threads.emplace_back(&NetworkManager::runReceiver, this, id, port, type);
        }
    };
    spawn(posePorts, 'p');
    spawn(lidarPorts, 'l');
    spawn(telemPorts, 't');
}

void NetworkManager::stop() {
    if (!running.exchange(false)) return;
    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }
    threads.clear();
}

bool NetworkManager::sendCommand(const std::string& /*roverId*/, uint8_t commandByte, int cmdPort) {
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cmdPort);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ssize_t n = sendto(sock, &commandByte, 1, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(sock);
    return n == 1;
}

StreamTimestamps NetworkManager::getStreamTimestamps(const std::string& roverId) const {
    std::lock_guard<std::mutex> lk(tsMutex);
    auto it = tsByRover.find(roverId);
    return it == tsByRover.end() ? StreamTimestamps{} : it->second;
}

void NetworkManager::runReceiver(const std::string& roverId, int port, char streamType) {
    int sock = createUdpSocketBind(port);
    if (sock < 0) return;
    std::vector<uint8_t> buffer(65536);

    while (running.load()) {
        ssize_t n = recv(sock, buffer.data(), buffer.size(), 0);
        if (n <= 0) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5ms);
            continue;
        }
        if (streamType == 'p' && static_cast<size_t>(n) >= sizeof(PosePacket)) {
            const auto* pkt = reinterpret_cast<const PosePacket*>(buffer.data());
            {
                std::lock_guard<std::mutex> lk(tsMutex);
                tsByRover[roverId].lastPoseTs = pkt->timestamp;
            }
            if (poseCb) poseCb(roverId, *pkt);
        } else if (streamType == 'l' && static_cast<size_t>(n) >= sizeof(LidarPacketHeader)) {
            const auto* hdr = reinterpret_cast<const LidarPacketHeader*>(buffer.data());
            size_t pts = hdr->pointsInThisChunk;
            const auto* ptsPtr = reinterpret_cast<const LidarPoint*>(buffer.data() + sizeof(LidarPacketHeader));
            {
                std::lock_guard<std::mutex> lk(tsMutex);
                tsByRover[roverId].lastLidarTs = hdr->timestamp;
            }
            if (lidarCb) lidarCb(roverId, *hdr, ptsPtr, pts);
        } else if (streamType == 't' && static_cast<size_t>(n) >= sizeof(VehicleTelem)) {
            const auto* v = reinterpret_cast<const VehicleTelem*>(buffer.data());
            {
                std::lock_guard<std::mutex> lk(tsMutex);
                tsByRover[roverId].lastTelemTs = v->timestamp;
            }
            if (telemCb) telemCb(roverId, *v);
        }
    }
    ::close(sock);
}


