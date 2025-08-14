#ifndef ROVER_RENDERER_H
#define ROVER_RENDERER_H

#include "Renderer.h"
#include "NetworkTypes.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * Rover visual representation data
 */
struct RoverVisual {
    int roverId;
    glm::vec3 position;
    glm::vec3 rotation;        // Rotation in degrees
    glm::vec4 color;           // RGBA color
    glm::mat4 modelMatrix;     // Cached transform matrix
    double lastUpdateTime;
    bool visible;
    bool selected;
    
    RoverVisual() 
        : roverId(0)
        , position(0.0f)
        , rotation(0.0f)
        , color(1.0f)
        , modelMatrix(1.0f)
        , lastUpdateTime(0.0)
        , visible(true)
        , selected(false) {}
};

/**
 * Rover geometry types
 */
enum class RoverGeometry {
    CUBE,           // Simple cube representation
    CYLINDER,       // Cylindrical body
    DETAILED        // More detailed rover model
};

/**
 * Rover rendering configuration
 */
struct RoverRenderConfig {
    RoverGeometry geometry;
    float scale;
    bool showTrails;
    bool showLabels;
    bool showSelection;
    float trailLength;
    int maxTrailPoints;
    
    RoverRenderConfig()
        : geometry(RoverGeometry::CUBE)
        , scale(1.0f)
        , showTrails(true)
        , showLabels(true)
        , showSelection(true)
        , trailLength(50.0f)
        , maxTrailPoints(100) {}
};

/**
 * Rover trail point for path visualization
 */
struct TrailPoint {
    glm::vec3 position;
    double timestamp;
    float alpha;
    
    TrailPoint() : position(0.0f), timestamp(0.0), alpha(1.0f) {}
    TrailPoint(const glm::vec3& pos, double time) 
        : position(pos), timestamp(time), alpha(1.0f) {}
};

/**
 * RoverRenderer - Handles rendering of rover models and trails
 * Supports multiple rovers with unique colors and efficient batch rendering
 */
class RoverRenderer {
public:
    RoverRenderer();
    ~RoverRenderer();

    /**
     * Initialize the rover renderer
     * @param renderer Shared renderer instance
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<Renderer> renderer);

    /**
     * Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * Update rover pose from network packet
     * @param roverId Rover ID
     * @param pose Pose packet data
     */
    void updateRoverPose(int roverId, const PosePacket& pose);

    /**
     * Update rover rendering (trails, animations, etc.)
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);

    /**
     * Render all rovers
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @param cameraPosition Camera position for depth sorting
     */
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, 
               const glm::vec3& cameraPosition);

    /**
     * Set rover visibility
     * @param roverId Rover ID (0 = all rovers)
     * @param visible Visibility state
     */
    void setRoverVisible(int roverId, bool visible);

    /**
     * Check if rover is visible
     * @param roverId Rover ID
     * @return true if visible
     */
    bool isRoverVisible(int roverId) const;

    /**
     * Set rover selection state
     * @param roverId Rover ID
     * @param selected Selection state
     */
    void setRoverSelected(int roverId, bool selected);

    /**
     * Get selected rover ID
     * @return Selected rover ID (0 = none)
     */
    int getSelectedRover() const;

    /**
     * Clear selection
     */
    void clearSelection();

    /**
     * Set rover color
     * @param roverId Rover ID
     * @param color RGBA color
     */
    void setRoverColor(int roverId, const glm::vec4& color);

    /**
     * Get rover color
     * @param roverId Rover ID
     * @return RGBA color
     */
    glm::vec4 getRoverColor(int roverId) const;

    /**
     * Get default rover colors (5 distinct colors for rovers 1-5)
     * @param roverId Rover ID
     * @return Default color for rover
     */
    static glm::vec4 getDefaultRoverColor(int roverId);

    /**
     * Set rendering configuration
     * @param config Rendering configuration
     */
    void setRenderConfig(const RoverRenderConfig& config);

    /**
     * Get rendering configuration
     * @return Current configuration
     */
    const RoverRenderConfig& getRenderConfig() const { return m_config; }

    /**
     * Set rover scale
     * @param scale Scale factor
     */
    void setRoverScale(float scale) { m_config.scale = scale; }

    /**
     * Enable/disable trail rendering
     * @param enabled Trail rendering enabled
     */
    void setTrailsEnabled(bool enabled) { m_config.showTrails = enabled; }

    /**
     * Enable/disable label rendering
     * @param enabled Label rendering enabled
     */
    void setLabelsEnabled(bool enabled) { m_config.showLabels = enabled; }

    /**
     * Clear all rover trails
     */
    void clearAllTrails();

    /**
     * Clear specific rover trail
     * @param roverId Rover ID
     */
    void clearRoverTrail(int roverId);

    /**
     * Get rover count
     * @return Number of active rovers
     */
    size_t getRoverCount() const { return m_rovers.size(); }

    /**
     * Get rover IDs
     * @return Vector of active rover IDs
     */
    std::vector<int> getRoverIds() const;

    /**
     * Get rover position
     * @param roverId Rover ID
     * @return Rover position (zero if not found)
     */
    glm::vec3 getRoverPosition(int roverId) const;

    /**
     * Get rover rotation
     * @param roverId Rover ID
     * @return Rover rotation in degrees (zero if not found)
     */
    glm::vec3 getRoverRotation(int roverId) const;

    /**
     * Check if rover exists
     * @param roverId Rover ID
     * @return true if rover exists
     */
    bool hasRover(int roverId) const;

    /**
     * Remove rover from rendering
     * @param roverId Rover ID
     */
    void removeRover(int roverId);

    /**
     * Get debug information
     * @return Debug string
     */
    std::string getDebugInfo() const;

private:
    // Renderer reference
    std::shared_ptr<Renderer> m_renderer;
    bool m_initialized;

    // Rover data
    std::unordered_map<int, RoverVisual> m_rovers;
    std::unordered_map<int, std::vector<TrailPoint>> m_trails;
    
    // Rendering configuration
    RoverRenderConfig m_config;

    // OpenGL resources
    GLuint m_cubeVAO, m_cubeVBO, m_cubeEBO;
    GLuint m_cylinderVAO, m_cylinderVBO, m_cylinderEBO;
    GLuint m_trailVAO, m_trailVBO;
    
    // Shader programs
    std::shared_ptr<Renderer::ShaderProgram> m_roverShader;
    std::shared_ptr<Renderer::ShaderProgram> m_trailShader;

    // Geometry data
    struct CubeGeometry {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
    } m_cubeGeometry;
    
    struct CylinderGeometry {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
    } m_cylinderGeometry;

    // Performance tracking
    double m_lastUpdateTime;
    int m_selectedRoverId;

    // Private methods
    bool createGeometry();
    bool createShaders();
    void generateCubeGeometry();
    void generateCylinderGeometry(int segments = 16);
    void setupVertexArrays();
    
    void renderRovers(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void renderTrails(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void renderLabels(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    void updateRoverMatrix(RoverVisual& rover);
    void updateTrails(float deltaTime);
    void addTrailPoint(int roverId, const glm::vec3& position, double timestamp);
    void trimTrail(std::vector<TrailPoint>& trail);
    
    glm::mat4 createModelMatrix(const glm::vec3& position, const glm::vec3& rotation, float scale) const;
    void sortRoversByDistance(const glm::vec3& cameraPosition, std::vector<int>& roverIds) const;
    
    // Geometry generation helpers
    void addCubeFace(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                    const glm::vec3& center, const glm::vec3& size, int faceIndex);
    void addCylinderVertex(std::vector<float>& vertices, const glm::vec3& position, 
                          const glm::vec3& normal, const glm::vec2& texCoord);
};

#endif // ROVER_RENDERER_H