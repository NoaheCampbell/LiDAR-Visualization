#pragma once

#include <vector>
#include <map>

#include <glm/glm.hpp>

#include "NetworkTypes.h"

struct RoverVisualState {
    glm::vec3 position {0.0f};
    glm::vec3 rotationDeg {0.0f};
    glm::vec3 color {1.0f, 1.0f, 1.0f};
};

class Renderer {
public:
    bool init();
    void shutdown();

    void resize(int width, int height);

    void updateRoverState(const std::string& roverId, const PosePacket& pose);
    void setRoverColor(const std::string& roverId, const glm::vec3& color);

    void setViewProjection(const glm::mat4& view, const glm::mat4& proj);

    void renderFrame(const std::vector<LidarPoint>& globalTerrain,
                     float fps,
                     int totalPoints);

private:
    unsigned int pointVbo = 0;
    unsigned int pointVao = 0;
    unsigned int roverVbo = 0;
    unsigned int roverVao = 0;
    unsigned int prog = 0;

    int viewportWidth = 1280;
    int viewportHeight = 720;

    std::map<std::string, RoverVisualState> rovers;

    unsigned int createShaderProgram();
    glm::mat4 viewM {1.0f};
    glm::mat4 projM {1.0f};
};


