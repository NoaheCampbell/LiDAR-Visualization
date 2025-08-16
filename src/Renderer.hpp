#pragma once

#include <vector>
#include <map>
#include <limits>
#include <functional>

#include <glm/glm.hpp>

#include "NetworkTypes.h"

struct TileUpdate; // fwd

struct RoverVisualState {
	glm::vec3 position {0.0f};
	glm::vec3 rotationDeg {0.0f};
	glm::vec3 color {1.0f, 1.0f, 1.0f};
	// Local-space offset (right, up, forward) meters, applied after terrain alignment
	glm::vec3 modelOffsetLocal {0.0f, 0.0f, 0.0f};
	// Smoothed state for rendering (to reduce visual jitter)
	glm::vec3 smoothedPosition {0.0f};
	glm::vec3 smoothedRotationDeg {0.0f};
	// Rolling estimate of pose jitter magnitude (meters)
	float noiseScore = 0.0f;
	// Ground filtering and vertical clamp
	bool initialized = false;
	float groundYFiltered = 0.0f;
	float lastRenderCenterY = 0.0f;

	// Alpha-Beta (g-h) filter state for pose
	bool kfInitialized = false;
	glm::vec3 kfPos {0.0f};
	glm::vec3 kfVel {0.0f};
	float kfYawDeg = 0.0f;
	float kfYawRateDeg = 0.0f;
	double lastPoseTs = 0.0;
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

	// Visualization controls
	// Height coloring is always on
	void setTerrainColorByHeight(bool) { /* no-op */ }
	bool getTerrainColorByHeight() const { return true; }
	float getObservedMinHeight() const { return observedMinY; }
	float getObservedMaxHeight() const { return observedMaxY; }
	void setRenderPoints(bool) { /* removed from UI */ }
	bool getRenderPoints() const { return renderPoints; }

	// Rover ground following (no physics)
	void setSnapRoversToGround(bool) { /* always on via symmetric limiter */ }
	bool getSnapRoversToGround() const { return true; }

	// Height range control
	void setAutoHeightRange(bool enabled) { autoHeightRange = enabled; }
	bool getAutoHeightRange() const { return autoHeightRange; }
	void setManualHeightRange(float minY, float maxY) { manualMinY = minY; manualMaxY = maxY; }
	void getManualHeightRange(float& minY, float& maxY) const { minY = manualMinY; maxY = manualMaxY; }
	void setHeightGradientColors(const glm::vec3& low, const glm::vec3& high) { lowColor = low; highColor = high; }
	void getHeightGradientColors(glm::vec3& low, glm::vec3& high) const { low = lowColor; high = highColor; }

	// Heightmap tile uploads and rendering
	void ensureTerrainPipeline(int gridNVertices);
	void uploadDirtyTiles(const std::vector<TileUpdate>& updates);
	void drawTerrain();

	// Toggle whether rovers align to terrain (height and normal). When false,
	// rovers are rendered at their smoothed pose position/orientation only.
	void setAlignToTerrain(bool enabled) { alignToTerrain = enabled; }
	bool getAlignToTerrain() const { return alignToTerrain; }

	// Provide elevation map sampling hook for grounding
	void setGroundSampler(std::function<bool(float,float,float&,uint16_t&)> sampler) { groundSampler = std::move(sampler); }

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

	std::function<bool(float,float,float&,uint16_t&)> groundSampler;

	// Terrain grid
	struct TileGpu {
		unsigned int vao = 0;
		unsigned int vbo = 0; // positions with y from height grid
		unsigned int ebo = 0; // shared index buffer per grid resolution
		int indexCount = 0;
		int tx = 0, tz = 0;
		float tileSize = 32.0f;
		float minY = std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
	};
	// key: (tx,tz) packed
	std::map<long long, TileGpu> gpuTiles;
	unsigned int terrainProg = 0;
	unsigned int sharedEbo = 0;
	int terrainGridN = 0;

	// Simple culling and overlay controls (tuned for perf)
	bool renderPoints = false; // disabled by default when terrain is on
	bool alignToTerrain = false; // default: do not interact with environment
	bool snapRoversToGround = false; // set rover Y to ground + offset without smoothing

	// Height-based coloring
	bool terrainColorByHeight = true;
	float observedMinY = std::numeric_limits<float>::infinity();
	float observedMaxY = -std::numeric_limits<float>::infinity();
	bool autoHeightRange = true;
	float manualMinY = 0.0f;
	float manualMaxY = 10.0f;
	glm::vec3 lowColor {0.2f, 0.4f, 0.95f};
	glm::vec3 highColor {0.95f, 0.35f, 0.2f};
	bool useVisibleHeightRange = true; // compute range from tiles near camera
	float lastVisibleMinY = std::numeric_limits<float>::infinity();
	float lastVisibleMaxY = -std::numeric_limits<float>::infinity();
};


