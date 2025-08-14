#ifndef POINT_CLOUD_RENDERER_H
#define POINT_CLOUD_RENDERER_H

#include "Renderer.h"
#include "TerrainManager.h"
#include "Camera.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

/**
 * Point rendering modes
 */
enum class PointRenderMode {
    SOLID,          // Solid colored points
    INTENSITY,      // Color based on intensity
    HEIGHT,         // Color based on height (Z coordinate)
    ROVER_ID,       // Color based on source rover ID
    TIMESTAMP       // Color based on timestamp (age)
};

/**
 * Level of detail settings for point cloud rendering
 */
struct PointCloudLOD {
    float nearDistance;     // Distance for full detail
    float farDistance;      // Distance for minimum detail
    float nearPointSize;    // Point size at near distance
    float farPointSize;     // Point size at far distance
    float skipRatio;        // Ratio of points to skip at far distance (0.0-1.0)
    
    PointCloudLOD()
        : nearDistance(50.0f)
        , farDistance(200.0f)
        , nearPointSize(2.0f)
        , farPointSize(1.0f)
        , skipRatio(0.8f) {}
};

/**
 * Point cloud rendering configuration
 */
struct PointCloudConfig {
    PointRenderMode renderMode;
    PointCloudLOD lod;
    bool enableCulling;         // Enable frustum culling
    bool enableLOD;             // Enable level of detail
    bool enableSizeAttenuation; // Enable distance-based point size
    float pointSizeMultiplier;  // Global point size multiplier
    float maxPointSize;         // Maximum point size
    float minPointSize;         // Minimum point size
    glm::vec4 defaultColor;     // Default point color
    float fadeTime;             // Point fade time in seconds (0 = no fade)
    bool showOnlyRecent;        // Show only recent points
    double recentTimeWindow;    // Time window for recent points
    
    PointCloudConfig()
        : renderMode(PointRenderMode::ROVER_ID)
        , enableCulling(true)
        , enableLOD(true)
        , enableSizeAttenuation(true)
        , pointSizeMultiplier(1.0f)
        , maxPointSize(8.0f)
        , minPointSize(1.0f)
        , defaultColor(1.0f, 1.0f, 1.0f, 1.0f)
        , fadeTime(30.0f)
        , showOnlyRecent(false)
        , recentTimeWindow(10.0) {}
};

/**
 * GPU point data structure for efficient rendering
 */
struct GPUPoint {
    glm::vec3 position;     // 3D position
    float intensity;        // Point intensity (0-1)
    glm::vec4 color;        // RGBA color
    float size;             // Point size
    float timestamp;        // Timestamp for fading
    int roverId;            // Source rover ID
    
    GPUPoint() 
        : position(0.0f)
        , intensity(1.0f)
        , color(1.0f)
        , size(1.0f)
        , timestamp(0.0f)
        , roverId(0) {}
        
    GPUPoint(const TerrainPoint& terrainPoint, float currentTime)
        : position(terrainPoint.x, terrainPoint.y, terrainPoint.z)
        , intensity(terrainPoint.intensity / 255.0f)
        , color(1.0f)
        , size(1.0f)
        , timestamp(static_cast<float>(currentTime - terrainPoint.timestamp))
        , roverId(terrainPoint.roverId) {}
};

/**
 * Point cloud rendering statistics
 */
struct PointCloudStats {
    size_t totalPoints;         // Total points in terrain
    size_t renderedPoints;      // Points actually rendered
    size_t culledPoints;        // Points culled by frustum
    size_t lodSkippedPoints;    // Points skipped by LOD
    double updateTime;          // Time spent updating point data (ms)
    double renderTime;          // Time spent rendering (ms)
    size_t gpuMemoryUsage;      // Estimated GPU memory usage (bytes)
    int activeRovers;           // Number of rovers contributing points
    
    PointCloudStats() {
        memset(this, 0, sizeof(*this));
    }
};

/**
 * High-performance point cloud renderer
 * Optimized for millions of points with LOD, culling, and GPU-based rendering
 */
class PointCloudRenderer {
public:
    PointCloudRenderer();
    ~PointCloudRenderer();

    /**
     * Initialize the point cloud renderer
     * @param renderer Shared renderer instance
     * @param terrainManager Terrain manager for point data
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<Renderer> renderer, 
                   std::shared_ptr<TerrainManager> terrainManager);

    /**
     * Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * Update point cloud data from terrain manager
     * @param camera Camera for frustum culling and LOD
     * @param deltaTime Time since last update
     */
    void update(const Camera& camera, float deltaTime);

    /**
     * Render point cloud
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param cameraPosition Camera position for distance calculations
     */
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
               const glm::vec3& cameraPosition);

    /**
     * Set rendering configuration
     * @param config New configuration
     */
    void setConfig(const PointCloudConfig& config);

    /**
     * Get rendering configuration
     * @return Current configuration
     */
    const PointCloudConfig& getConfig() const { return m_config; }

    /**
     * Set point render mode
     * @param mode Render mode
     */
    void setRenderMode(PointRenderMode mode) { m_config.renderMode = mode; }

    /**
     * Get point render mode
     * @return Current render mode
     */
    PointRenderMode getRenderMode() const { return m_config.renderMode; }

    /**
     * Set point size multiplier
     * @param multiplier Size multiplier
     */
    void setPointSizeMultiplier(float multiplier) { m_config.pointSizeMultiplier = multiplier; }

    /**
     * Enable/disable frustum culling
     * @param enabled Culling enabled
     */
    void setCullingEnabled(bool enabled) { m_config.enableCulling = enabled; }

    /**
     * Enable/disable level of detail
     * @param enabled LOD enabled
     */
    void setLODEnabled(bool enabled) { m_config.enableLOD = enabled; }

    /**
     * Set fade time for points
     * @param fadeTime Fade time in seconds (0 = no fade)
     */
    void setFadeTime(float fadeTime) { m_config.fadeTime = fadeTime; }

    /**
     * Enable/disable recent points only mode
     * @param enabled Show only recent points
     * @param timeWindow Time window in seconds
     */
    void setShowOnlyRecent(bool enabled, double timeWindow = 10.0) {
        m_config.showOnlyRecent = enabled;
        m_config.recentTimeWindow = timeWindow;
    }

    /**
     * Set rover visibility
     * @param roverId Rover ID (0 = all rovers)
     * @param visible Visibility state
     */
    void setRoverVisible(int roverId, bool visible);

    /**
     * Check if rover points are visible
     * @param roverId Rover ID
     * @return true if visible
     */
    bool isRoverVisible(int roverId) const;

    /**
     * Set color for rover points
     * @param roverId Rover ID
     * @param color RGBA color
     */
    void setRoverColor(int roverId, const glm::vec4& color);

    /**
     * Get color for rover points
     * @param roverId Rover ID
     * @return RGBA color
     */
    glm::vec4 getRoverColor(int roverId) const;

    /**
     * Clear all point cloud data
     */
    void clearAll();

    /**
     * Clear points from specific rover
     * @param roverId Rover ID
     */
    void clearRover(int roverId);

    /**
     * Get rendering statistics
     * @return Statistics structure
     */
    PointCloudStats getStats() const;

    /**
     * Get estimated GPU memory usage
     * @return Memory usage in bytes
     */
    size_t getGPUMemoryUsage() const;

    /**
     * Force update of point data from terrain manager
     */
    void forceUpdate();

    /**
     * Set LOD configuration
     * @param lod LOD settings
     */
    void setLODConfig(const PointCloudLOD& lod) { m_config.lod = lod; }

    /**
     * Get debug information
     * @return Debug string
     */
    std::string getDebugInfo() const;

private:
    // Core components
    std::shared_ptr<Renderer> m_renderer;
    std::shared_ptr<TerrainManager> m_terrainManager;
    bool m_initialized;

    // Configuration
    PointCloudConfig m_config;

    // OpenGL resources
    GLuint m_pointVAO;
    GLuint m_pointVBO;
    std::shared_ptr<Renderer::ShaderProgram> m_pointShader;

    // Point data management
    std::vector<GPUPoint> m_points;
    std::vector<GPUPoint> m_renderPoints;
    mutable std::mutex m_pointsMutex;
    std::atomic<bool> m_dataChanged;
    
    // Rover management
    std::unordered_map<int, bool> m_roverVisibility;
    std::unordered_map<int, glm::vec4> m_roverColors;

    // Performance tracking
    mutable PointCloudStats m_stats;
    double m_lastUpdateTime;
    double m_lastRenderTime;
    
    // Culling and LOD
    Frustum m_lastFrustum;
    glm::vec3 m_lastCameraPosition;
    bool m_frustumValid;

    // Buffer management
    size_t m_bufferSize;
    size_t m_maxBufferSize;

    // Private methods
    bool createShaders();
    bool setupVertexArrays();
    
    void updatePointData(const Camera& camera, float deltaTime);
    void updateFromTerrainManager();
    void applyCulling(const Frustum& frustum);
    void applyLOD(const glm::vec3& cameraPosition);
    void applyColorAndSize(float currentTime);
    void uploadToGPU();
    
    glm::vec4 calculatePointColor(const GPUPoint& point, float currentTime) const;
    float calculatePointSize(const GPUPoint& point, const glm::vec3& cameraPosition) const;
    float calculateDistanceAttenuation(float distance) const;
    
    bool shouldRenderPoint(const GPUPoint& point, float currentTime) const;
    bool isPointInFrustum(const glm::vec3& position, const Frustum& frustum) const;
    
    void updateStats();
    size_t calculateGPUMemoryUsage() const;
    
    // Color mapping functions
    glm::vec4 getColorByRoverId(int roverId) const;
    glm::vec4 getColorByHeight(float height) const;
    glm::vec4 getColorByIntensity(float intensity) const;
    glm::vec4 getColorByTimestamp(float timestamp, float currentTime) const;
    
    // Default rover colors
    static glm::vec4 getDefaultRoverColor(int roverId);
};

#endif // POINT_CLOUD_RENDERER_H