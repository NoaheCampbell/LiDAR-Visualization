#include "Camera.h"
#include "Constants.h"
#include "Logger.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <sstream>
#include <unordered_map>

// Frustum implementation
bool Frustum::containsPoint(const glm::vec3& point) const {
    for (int i = 0; i < 6; i++) {
        if (glm::dot(glm::vec3(planes[i]), point) + planes[i].w < 0) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersectsSphere(const glm::vec3& center, float radius) const {
    for (int i = 0; i < 6; i++) {
        float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
    for (int i = 0; i < 6; i++) {
        glm::vec3 normal = glm::vec3(planes[i]);
        glm::vec3 positiveVertex = min;
        
        if (normal.x >= 0) positiveVertex.x = max.x;
        if (normal.y >= 0) positiveVertex.y = max.y;
        if (normal.z >= 0) positiveVertex.z = max.z;
        
        if (glm::dot(normal, positiveVertex) + planes[i].w < 0) {
            return false;
        }
    }
    return true;
}

// Camera implementation
Camera::Camera()
    : m_mode(CameraMode::FREE_FLY)
    , m_position(0.0f, 5.0f, 10.0f)
    , m_forward(0.0f, 0.0f, -1.0f)
    , m_right(1.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_worldUp(0.0f, 1.0f, 0.0f)
    , m_yaw(-90.0f)
    , m_pitch(0.0f)
    , m_fov(RenderConfig::DEFAULT_FOV)
    , m_nearPlane(RenderConfig::NEAR_PLANE)
    , m_farPlane(RenderConfig::FAR_PLANE)
    , m_movementSpeed(RenderConfig::CAMERA_MOVE_SPEED)
    , m_rotationSpeed(RenderConfig::CAMERA_ROTATE_SPEED)
    , m_zoomSpeed(RenderConfig::CAMERA_ZOOM_SPEED)
    , m_followRoverId(0)
    , m_followDistance(15.0f)
    , m_followHeight(5.0f)
    , m_followOffset(0.0f)
    , m_mouseCapture(false)
    , m_firstMouse(true)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    , m_mousePressed(false) {
    
    // Initialize key states
    std::fill(m_keyStates, m_keyStates + 1024, false);
}

Camera::~Camera() {
}

void Camera::initialize(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    m_position = position;
    m_worldUp = up;
    
    // Calculate initial orientation from position and target
    glm::vec3 direction = glm::normalize(target - position);
    m_yaw = glm::degrees(atan2(direction.z, direction.x));
    m_pitch = glm::degrees(asin(direction.y));
    
    updateCameraVectors();
    
    Logger::info("Camera initialized at position (" + 
                std::to_string(position.x) + ", " + 
                std::to_string(position.y) + ", " + 
                std::to_string(position.z) + ")");
}

void Camera::update(float deltaTime) {
    updateTransition(deltaTime);
    
    switch (m_mode) {
        case CameraMode::FREE_FLY:
            updateFreeFlyMovement(deltaTime);
            break;
        case CameraMode::FOLLOW:
            updateFollowMode(deltaTime);
            break;
        case CameraMode::ORBIT:
            // Orbit mode not implemented yet
            break;
    }
    
    updateCameraVectors();
}

void Camera::handleKeyboard(int key, int action, float deltaTime) {
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            m_keyStates[key] = true;
        } else if (action == GLFW_RELEASE) {
            m_keyStates[key] = false;
        }
    }
    
    // Handle mode switching
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_F1:
                setMode(CameraMode::FREE_FLY);
                break;
            case GLFW_KEY_F2:
                setMode(CameraMode::FOLLOW);
                break;
            case GLFW_KEY_R:
                reset();
                break;
        }
    }
}

void Camera::handleMouseMovement(double xpos, double ypos, float deltaTime) {
    if (m_firstMouse) {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        m_firstMouse = false;
        return;
    }

    double xoffset = xpos - m_lastMouseX;
    double yoffset = m_lastMouseY - ypos; // Reversed since y-coordinates go from bottom to top
    m_lastMouseX = xpos;
    m_lastMouseY = ypos;

    if (m_mode == CameraMode::FREE_FLY && (m_mouseCapture || m_mousePressed)) {
        float sensitivity = m_rotationSpeed * deltaTime;
        m_yaw += static_cast<float>(xoffset) * sensitivity;
        m_pitch += static_cast<float>(yoffset) * sensitivity;

        // Constrain pitch
        m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    }
}

void Camera::handleMouseScroll(double yoffset) {
    if (m_mode == CameraMode::FREE_FLY) {
        m_fov -= static_cast<float>(yoffset) * m_zoomSpeed;
        m_fov = glm::clamp(m_fov, 1.0f, 120.0f);
    } else if (m_mode == CameraMode::FOLLOW) {
        m_followDistance -= static_cast<float>(yoffset) * m_zoomSpeed;
        m_followDistance = glm::clamp(m_followDistance, 5.0f, 100.0f);
    }
}

void Camera::handleMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        m_mousePressed = (action == GLFW_PRESS);
    }
}

void Camera::setMode(CameraMode mode, bool smoothTransition) {
    if (m_mode == mode) {
        return;
    }

    CameraMode oldMode = m_mode;
    m_mode = mode;
    
    Logger::info("Camera mode changed to " + std::to_string(static_cast<int>(mode)));
    
    if (smoothTransition && !m_transition.inProgress) {
        // Setup smooth transition between modes
        m_transition.inProgress = true;
        m_transition.duration = 1.0f;
        m_transition.elapsed = 0.0f;
        m_transition.startPosition = m_position;
        m_transition.startOrientation = orientationFromAngles(m_yaw, m_pitch);
        
        // Calculate target based on new mode
        if (mode == CameraMode::FOLLOW && m_followRoverId > 0) {
            auto it = m_roverPoses.find(m_followRoverId);
            if (it != m_roverPoses.end() && it->second.valid) {
                m_transition.targetPosition = calculateFollowPosition(it->second);
            }
        }
    }
}

void Camera::setFollowTarget(int roverId) {
    m_followRoverId = roverId;
    if (roverId > 0) {
        Logger::info("Camera following rover " + std::to_string(roverId));
    } else {
        Logger::info("Camera follow target cleared");
    }
}

void Camera::updateRoverPose(int roverId, const PosePacket& pose) {
    RoverPose& roverPose = m_roverPoses[roverId];
    roverPose.position = glm::vec3(pose.posX, pose.posY, pose.posZ);
    roverPose.rotation = glm::vec3(pose.rotXdeg, pose.rotYdeg, pose.rotZdeg);
    roverPose.timestamp = pose.timestamp;
    roverPose.valid = true;
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
}

void Camera::setPositionSmooth(const glm::vec3& position, float duration) {
    m_transition.inProgress = true;
    m_transition.duration = duration;
    m_transition.elapsed = 0.0f;
    m_transition.startPosition = m_position;
    m_transition.targetPosition = position;
    m_transition.startOrientation = orientationFromAngles(m_yaw, m_pitch);
    m_transition.targetOrientation = m_transition.startOrientation;
}

void Camera::setTarget(const glm::vec3& target) {
    glm::vec3 direction = glm::normalize(target - m_position);
    m_yaw = glm::degrees(atan2(direction.z, direction.x));
    m_pitch = glm::degrees(asin(direction.y));
}

void Camera::setTargetSmooth(const glm::vec3& target, float duration) {
    glm::vec3 direction = glm::normalize(target - m_position);
    float targetYaw = glm::degrees(atan2(direction.z, direction.x));
    float targetPitch = glm::degrees(asin(direction.y));
    
    m_transition.inProgress = true;
    m_transition.duration = duration;
    m_transition.elapsed = 0.0f;
    m_transition.startPosition = m_position;
    m_transition.targetPosition = m_position;
    m_transition.startOrientation = orientationFromAngles(m_yaw, m_pitch);
    m_transition.targetOrientation = orientationFromAngles(targetYaw, targetPitch);
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_forward, m_up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
}

glm::mat4 Camera::getViewProjectionMatrix(float aspectRatio) const {
    return getProjectionMatrix(aspectRatio) * getViewMatrix();
}

Frustum Camera::getViewFrustum(float aspectRatio) const {
    Frustum frustum;
    glm::mat4 viewProjection = getViewProjectionMatrix(aspectRatio);
    extractFrustumPlanes(viewProjection, frustum);
    return frustum;
}

void Camera::setFieldOfView(float fov) {
    m_fov = glm::clamp(fov, 1.0f, 179.0f);
}

void Camera::setClippingPlanes(float nearPlane, float farPlane) {
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::reset() {
    m_position = glm::vec3(0.0f, 5.0f, 10.0f);
    m_yaw = -90.0f;
    m_pitch = 0.0f;
    m_fov = RenderConfig::DEFAULT_FOV;
    m_followRoverId = 0;
    m_transition.inProgress = false;
    updateCameraVectors();
    Logger::info("Camera reset to default position");
}

std::string Camera::getDebugInfo() const {
    std::stringstream ss;
    ss << "Camera Mode: " << static_cast<int>(m_mode) << "\n";
    ss << "Position: (" << m_position.x << ", " << m_position.y << ", " << m_position.z << ")\n";
    ss << "Yaw: " << m_yaw << ", Pitch: " << m_pitch << "\n";
    ss << "FOV: " << m_fov << "\n";
    if (m_mode == CameraMode::FOLLOW) {
        ss << "Follow Target: " << m_followRoverId << "\n";
        ss << "Follow Distance: " << m_followDistance << "\n";
    }
    return ss.str();
}

// Private methods
void Camera::updateCameraVectors() {
    // Calculate forward vector from yaw and pitch
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward = glm::normalize(front);
    
    // Calculate right and up vectors
    m_right = glm::normalize(glm::cross(m_forward, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

void Camera::updateFreeFlyMovement(float deltaTime) {
    processKeyboardInput(deltaTime);
}

void Camera::updateFollowMode(float deltaTime) {
    if (m_followRoverId == 0) {
        return;
    }
    
    auto it = m_roverPoses.find(m_followRoverId);
    if (it == m_roverPoses.end() || !it->second.valid) {
        return;
    }
    
    const RoverPose& roverPose = it->second;
    glm::vec3 targetPosition = calculateFollowPosition(roverPose);
    
    // Smooth camera movement towards target
    float smoothing = 5.0f; // Adjustment speed
    m_position += (targetPosition - m_position) * smoothing * deltaTime;
    
    // Look at the rover
    glm::vec3 direction = glm::normalize(roverPose.position - m_position);
    m_yaw = glm::degrees(atan2(direction.z, direction.x));
    m_pitch = glm::degrees(asin(direction.y));
}

void Camera::updateTransition(float deltaTime) {
    if (!m_transition.inProgress) {
        return;
    }
    
    m_transition.elapsed += deltaTime;
    float t = m_transition.elapsed / m_transition.duration;
    
    if (t >= 1.0f) {
        // Transition complete
        t = 1.0f;
        m_transition.inProgress = false;
    }
    
    // Apply smooth interpolation
    float smoothT = smoothStep(t);
    
    // Interpolate position
    m_position = glm::mix(m_transition.startPosition, m_transition.targetPosition, smoothT);
    
    // Interpolate orientation
    glm::quat currentOrientation = glm::slerp(m_transition.startOrientation, 
                                             m_transition.targetOrientation, smoothT);
    anglesFromOrientation(currentOrientation, m_yaw, m_pitch);
}

void Camera::processKeyboardInput(float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;
    
    if (m_keyStates[GLFW_KEY_W] || m_keyStates[GLFW_KEY_UP]) {
        m_position += m_forward * velocity;
    }
    if (m_keyStates[GLFW_KEY_S] || m_keyStates[GLFW_KEY_DOWN]) {
        m_position -= m_forward * velocity;
    }
    if (m_keyStates[GLFW_KEY_A] || m_keyStates[GLFW_KEY_LEFT]) {
        m_position -= m_right * velocity;
    }
    if (m_keyStates[GLFW_KEY_D] || m_keyStates[GLFW_KEY_RIGHT]) {
        m_position += m_right * velocity;
    }
    if (m_keyStates[GLFW_KEY_Q] || m_keyStates[GLFW_KEY_PAGE_DOWN]) {
        m_position -= m_worldUp * velocity;
    }
    if (m_keyStates[GLFW_KEY_E] || m_keyStates[GLFW_KEY_PAGE_UP]) {
        m_position += m_worldUp * velocity;
    }
    
    // Speed adjustment
    if (m_keyStates[GLFW_KEY_LEFT_SHIFT]) {
        velocity *= 3.0f; // Boost speed
    }
    if (m_keyStates[GLFW_KEY_LEFT_CONTROL]) {
        velocity *= 0.3f; // Slow down
    }
}

glm::vec3 Camera::calculateFollowPosition(const RoverPose& roverPose) const {
    // Calculate position behind and above the rover
    glm::vec3 roverForward = glm::vec3(
        cos(glm::radians(roverPose.rotation.z)), 
        0.0f,
        sin(glm::radians(roverPose.rotation.z))
    );
    
    glm::vec3 offset = -roverForward * m_followDistance + glm::vec3(0.0f, m_followHeight, 0.0f);
    return roverPose.position + offset;
}

void Camera::extractFrustumPlanes(const glm::mat4& viewProjection, Frustum& frustum) const {
    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum.planes[0] = glm::vec4(
        viewProjection[0][3] + viewProjection[0][0],
        viewProjection[1][3] + viewProjection[1][0],
        viewProjection[2][3] + viewProjection[2][0],
        viewProjection[3][3] + viewProjection[3][0]
    );
    
    // Right plane
    frustum.planes[1] = glm::vec4(
        viewProjection[0][3] - viewProjection[0][0],
        viewProjection[1][3] - viewProjection[1][0],
        viewProjection[2][3] - viewProjection[2][0],
        viewProjection[3][3] - viewProjection[3][0]
    );
    
    // Bottom plane
    frustum.planes[2] = glm::vec4(
        viewProjection[0][3] + viewProjection[0][1],
        viewProjection[1][3] + viewProjection[1][1],
        viewProjection[2][3] + viewProjection[2][1],
        viewProjection[3][3] + viewProjection[3][1]
    );
    
    // Top plane
    frustum.planes[3] = glm::vec4(
        viewProjection[0][3] - viewProjection[0][1],
        viewProjection[1][3] - viewProjection[1][1],
        viewProjection[2][3] - viewProjection[2][1],
        viewProjection[3][3] - viewProjection[3][1]
    );
    
    // Near plane
    frustum.planes[4] = glm::vec4(
        viewProjection[0][3] + viewProjection[0][2],
        viewProjection[1][3] + viewProjection[1][2],
        viewProjection[2][3] + viewProjection[2][2],
        viewProjection[3][3] + viewProjection[3][2]
    );
    
    // Far plane
    frustum.planes[5] = glm::vec4(
        viewProjection[0][3] - viewProjection[0][2],
        viewProjection[1][3] - viewProjection[1][2],
        viewProjection[2][3] - viewProjection[2][2],
        viewProjection[3][3] - viewProjection[3][2]
    );
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(frustum.planes[i]));
        frustum.planes[i] /= length;
    }
}

glm::quat Camera::orientationFromAngles(float yaw, float pitch) const {
    glm::quat pitchQuat = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yawQuat = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    return yawQuat * pitchQuat;
}

void Camera::anglesFromOrientation(const glm::quat& orientation, float& yaw, float& pitch) const {
    glm::vec3 eulerAngles = glm::eulerAngles(orientation);
    yaw = glm::degrees(eulerAngles.y);
    pitch = glm::degrees(eulerAngles.x);
}

float Camera::smoothStep(float t) const {
    return t * t * (3.0f - 2.0f * t);
}