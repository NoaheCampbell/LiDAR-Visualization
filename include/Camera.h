#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "NetworkTypes.h"
#include <memory>

/**
 * Camera modes for different viewing behaviors
 */
enum class CameraMode {
    FREE_FLY,   // Free-flying camera with WASD + mouse controls
    FOLLOW,     // Camera follows selected rover
    ORBIT       // Camera orbits around a target point
};

/**
 * Frustum planes for view frustum culling
 */
struct Frustum {
    glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
    
    /**
     * Check if a point is inside the frustum
     * @param point Point to test
     * @return true if point is inside frustum
     */
    bool containsPoint(const glm::vec3& point) const;
    
    /**
     * Check if a sphere is inside or intersecting the frustum
     * @param center Sphere center
     * @param radius Sphere radius
     * @return true if sphere intersects frustum
     */
    bool intersectsSphere(const glm::vec3& center, float radius) const;
    
    /**
     * Check if an AABB is inside or intersecting the frustum
     * @param min AABB minimum point
     * @param max AABB maximum point
     * @return true if AABB intersects frustum
     */
    bool intersectsAABB(const glm::vec3& min, const glm::vec3& max) const;
};

/**
 * Camera interpolation settings
 */
struct CameraTransition {
    bool inProgress;
    float duration;
    float elapsed;
    glm::vec3 startPosition;
    glm::vec3 targetPosition;
    glm::quat startOrientation;
    glm::quat targetOrientation;
    
    CameraTransition() : inProgress(false), duration(0.0f), elapsed(0.0f) {}
};

/**
 * Camera class supporting multiple modes and smooth transitions
 * Optimized for LiDAR visualization with frustum culling support
 */
class Camera {
public:
    Camera();
    ~Camera();

    /**
     * Initialize camera with default settings
     * @param position Initial camera position
     * @param target Initial look-at target
     * @param up Initial up vector
     */
    void initialize(const glm::vec3& position = glm::vec3(0.0f, 5.0f, 10.0f),
                   const glm::vec3& target = glm::vec3(0.0f, 0.0f, 0.0f),
                   const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

    /**
     * Update camera logic (interpolation, following, etc.)
     * @param deltaTime Time since last update in seconds
     */
    void update(float deltaTime);

    /**
     * Handle keyboard input for camera movement
     * @param key Key code (GLFW key constants)
     * @param action Key action (GLFW_PRESS, GLFW_RELEASE, GLFW_REPEAT)
     * @param deltaTime Time since last update
     */
    void handleKeyboard(int key, int action, float deltaTime);

    /**
     * Handle mouse movement for camera rotation
     * @param xpos Mouse X position
     * @param ypos Mouse Y position
     * @param deltaTime Time since last update
     */
    void handleMouseMovement(double xpos, double ypos, float deltaTime);

    /**
     * Handle mouse scroll for zoom/movement speed
     * @param yoffset Scroll wheel offset
     */
    void handleMouseScroll(double yoffset);

    /**
     * Handle mouse button input
     * @param button Mouse button
     * @param action Button action
     */
    void handleMouseButton(int button, int action);

    /**
     * Set camera mode
     * @param mode New camera mode
     * @param smoothTransition Use smooth transition
     */
    void setMode(CameraMode mode, bool smoothTransition = true);

    /**
     * Get current camera mode
     * @return Current mode
     */
    CameraMode getMode() const { return m_mode; }

    /**
     * Set follow target rover ID
     * @param roverId Rover ID to follow (0 = none)
     */
    void setFollowTarget(int roverId);

    /**
     * Get current follow target
     * @return Rover ID being followed (0 = none)
     */
    int getFollowTarget() const { return m_followRoverId; }

    /**
     * Update rover pose for follow mode
     * @param roverId Rover ID
     * @param pose Rover pose packet
     */
    void updateRoverPose(int roverId, const PosePacket& pose);

    /**
     * Set camera position (immediate)
     * @param position New position
     */
    void setPosition(const glm::vec3& position);

    /**
     * Set camera position (smooth transition)
     * @param position Target position
     * @param duration Transition duration in seconds
     */
    void setPositionSmooth(const glm::vec3& position, float duration = 1.0f);

    /**
     * Set camera target/look-at point
     * @param target Target position
     */
    void setTarget(const glm::vec3& target);

    /**
     * Set camera target (smooth transition)
     * @param target Target position
     * @param duration Transition duration in seconds
     */
    void setTargetSmooth(const glm::vec3& target, float duration = 1.0f);

    /**
     * Get camera position
     * @return Current position
     */
    glm::vec3 getPosition() const { return m_position; }

    /**
     * Get camera forward direction
     * @return Forward vector
     */
    glm::vec3 getForward() const { return m_forward; }

    /**
     * Get camera right direction
     * @return Right vector
     */
    glm::vec3 getRight() const { return m_right; }

    /**
     * Get camera up direction
     * @return Up vector
     */
    glm::vec3 getUp() const { return m_up; }

    /**
     * Get view matrix
     * @return 4x4 view matrix
     */
    glm::mat4 getViewMatrix() const;

    /**
     * Get projection matrix
     * @param aspectRatio Screen aspect ratio
     * @return 4x4 projection matrix
     */
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    /**
     * Get view-projection matrix
     * @param aspectRatio Screen aspect ratio
     * @return Combined view-projection matrix
     */
    glm::mat4 getViewProjectionMatrix(float aspectRatio) const;

    /**
     * Get view frustum for culling
     * @param aspectRatio Screen aspect ratio
     * @return View frustum
     */
    Frustum getViewFrustum(float aspectRatio) const;

    /**
     * Set field of view
     * @param fov Field of view in degrees
     */
    void setFieldOfView(float fov);

    /**
     * Get field of view
     * @return FOV in degrees
     */
    float getFieldOfView() const { return m_fov; }

    /**
     * Set near and far clipping planes
     * @param nearPlane Near clipping distance
     * @param farPlane Far clipping distance
     */
    void setClippingPlanes(float nearPlane, float farPlane);

    /**
     * Get near clipping plane
     * @return Near plane distance
     */
    float getNearPlane() const { return m_nearPlane; }

    /**
     * Get far clipping plane
     * @return Far plane distance
     */
    float getFarPlane() const { return m_farPlane; }

    /**
     * Set movement speed
     * @param speed Movement speed in units/second
     */
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }

    /**
     * Get movement speed
     * @return Current movement speed
     */
    float getMovementSpeed() const { return m_movementSpeed; }

    /**
     * Set rotation speed
     * @param speed Rotation speed in degrees/second
     */
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }

    /**
     * Get rotation speed
     * @return Current rotation speed
     */
    float getRotationSpeed() const { return m_rotationSpeed; }

    /**
     * Set zoom speed
     * @param speed Zoom speed multiplier
     */
    void setZoomSpeed(float speed) { m_zoomSpeed = speed; }

    /**
     * Get zoom speed
     * @return Current zoom speed
     */
    float getZoomSpeed() const { return m_zoomSpeed; }

    /**
     * Set follow distance for follow mode
     * @param distance Distance to maintain from rover
     */
    void setFollowDistance(float distance) { m_followDistance = distance; }

    /**
     * Get follow distance
     * @return Current follow distance
     */
    float getFollowDistance() const { return m_followDistance; }

    /**
     * Set follow height offset
     * @param height Height offset above rover
     */
    void setFollowHeight(float height) { m_followHeight = height; }

    /**
     * Get follow height
     * @return Current follow height
     */
    float getFollowHeight() const { return m_followHeight; }

    /**
     * Enable/disable mouse capture for free-fly mode
     * @param capture Capture mouse input
     */
    void setMouseCapture(bool capture) { m_mouseCapture = capture; }

    /**
     * Check if mouse is captured
     * @return true if mouse is captured
     */
    bool isMouseCaptured() const { return m_mouseCapture; }

    /**
     * Reset camera to default position and orientation
     */
    void reset();

    /**
     * Get camera information for debugging
     * @return String with camera state info
     */
    std::string getDebugInfo() const;

private:
    // Camera state
    CameraMode m_mode;
    glm::vec3 m_position;
    glm::vec3 m_forward;
    glm::vec3 m_right;
    glm::vec3 m_up;
    glm::vec3 m_worldUp;

    // Spherical coordinates for free-fly mode
    float m_yaw;    // Horizontal rotation
    float m_pitch;  // Vertical rotation

    // Projection settings
    float m_fov;
    float m_nearPlane;
    float m_farPlane;

    // Movement settings
    float m_movementSpeed;
    float m_rotationSpeed;
    float m_zoomSpeed;

    // Follow mode settings
    int m_followRoverId;
    float m_followDistance;
    float m_followHeight;
    glm::vec3 m_followOffset;

    // Mouse handling
    bool m_mouseCapture;
    bool m_firstMouse;
    double m_lastMouseX;
    double m_lastMouseY;
    bool m_mousePressed;

    // Key states for smooth movement
    bool m_keyStates[1024];

    // Rover pose tracking for follow mode
    struct RoverPose {
        glm::vec3 position;
        glm::vec3 rotation; // In degrees
        double timestamp;
        bool valid;
        
        RoverPose() : position(0.0f), rotation(0.0f), timestamp(0.0), valid(false) {}
    };
    std::unordered_map<int, RoverPose> m_roverPoses;

    // Smooth transitions
    CameraTransition m_transition;

    // Helper methods
    void updateCameraVectors();
    void updateFreeFlyMovement(float deltaTime);
    void updateFollowMode(float deltaTime);
    void updateTransition(float deltaTime);
    void processKeyboardInput(float deltaTime);
    glm::vec3 calculateFollowPosition(const RoverPose& roverPose) const;
    void extractFrustumPlanes(const glm::mat4& viewProjection, Frustum& frustum) const;
    glm::quat orientationFromAngles(float yaw, float pitch) const;
    void anglesFromOrientation(const glm::quat& orientation, float& yaw, float& pitch) const;
    float smoothStep(float t) const;
};

#endif // CAMERA_H