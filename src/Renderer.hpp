#pragma once

#include <vector>
#include <map>

#include <glm/glm.hpp>

#include "NetworkTypes.h"
struct TileUpdate; // fwd

struct RoverVisualState {
    glm::vec3 position {0.0f};
    glm::vec3 rotationDeg {0.0f};
    glm::vec3 color {1.0f, 1.0f, 1.0f};
    // Local-space offset (right, up, forward) meters, applied after terrain alignment
    glm::vec3 modelOffsetLocal {0.0f, 0.0f, 0.0f};
};

class Renderer {
public:
    bool init();
    void shutdown();

    void resize(int width, int height);

    void updateRoverState(const std::string& roverId, const PosePacket& pose);
    void setRoverColor(const std::string& roverId, const glm::vec3& color);
    void setRoverModelOffset(const std::string& roverId, const glm::vec3& localOffsetRUF);

    void setViewProjection(const glm::mat4& view, const glm::mat4& proj);

    void renderFrame(const std::vector<LidarPoint>& globalTerrain,
                     float fps,
                     int totalPoints);
    void setTerrainDrawDistance(float meters) { terrainDrawDistance = meters; }
    float getTerrainDrawDistance() const { return terrainDrawDistance; }

    // Heightmap tile uploads and rendering
    void ensureTerrainPipeline(int gridNVertices);
    void uploadDirtyTiles(const std::vector<TileUpdate>& updates);
    void drawTerrain();

private:
    unsigned int pointVbo = 0;
    unsigned int pointVao = 0;
    // Legacy rover point rendering (kept for reference/optional use)
    unsigned int roverVbo = 0;
    unsigned int roverVao = 0;
    unsigned int roverLineVbo = 0;
    unsigned int roverLineVao = 0;
    unsigned int prog = 0;
    // Rover 3D mesh (simple cube)
    unsigned int roverMeshVao = 0;
    unsigned int roverMeshVbo = 0;
    unsigned int roverMeshEbo = 0;
    int roverMeshIndexCount = 0;

    int viewportWidth = 1280;
    int viewportHeight = 720;
    float terrainDrawDistance = 1200.0f;

    std::map<std::string, RoverVisualState> rovers;

    unsigned int createShaderProgram();
    glm::mat4 viewM {1.0f};
    glm::mat4 projM {1.0f};

public:
    // Provide elevation map sampling hook for grounding
    void setGroundSampler(std::function<bool(float,float,float&,uint16_t&)> sampler) { groundSampler = std::move(sampler); }
private:
    std::function<bool(float,float,float&,uint16_t&)> groundSampler;

    // Terrain grid
    struct TileGpu {
        unsigned int vao = 0;
        unsigned int vbo = 0; // positions with y from height grid
        unsigned int ebo = 0; // shared index buffer per grid resolution
        int indexCount = 0;
        int tx = 0, tz = 0;
        float tileSize = 32.0f;
    };
    // key: (tx,tz) packed
    std::map<long long, TileGpu> gpuTiles;
    unsigned int terrainProg = 0;
    unsigned int sharedEbo = 0;
    int terrainGridN = 0;

    // Simple culling and overlay controls (tuned for perf)
    bool renderPoints = false; // disabled by default when terrain is on
};


