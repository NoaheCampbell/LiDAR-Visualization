#include "PointCloudRenderer.h"
#include "Constants.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <sstream>

PointCloudRenderer::PointCloudRenderer()
    : m_initialized(false)
    , m_pointVAO(0)
    , m_pointVBO(0)
    , m_dataChanged(false)
    , m_lastUpdateTime(0.0)
    , m_lastRenderTime(0.0)
    , m_frustumValid(false)
    , m_bufferSize(0)
    , m_maxBufferSize(RenderConfig::POINT_BUFFER_SIZE) {
}

PointCloudRenderer::~PointCloudRenderer() {
    shutdown();
}

bool PointCloudRenderer::initialize(std::shared_ptr<Renderer> renderer, 
                                   std::shared_ptr<TerrainManager> terrainManager) {
    if (m_initialized) {
        Logger::warning("PointCloudRenderer already initialized");
        return true;
    }

    m_renderer = renderer;
    m_terrainManager = terrainManager;
    
    if (!m_renderer) {
        Logger::error("Invalid renderer passed to PointCloudRenderer");
        return false;
    }
    
    if (!m_terrainManager) {
        Logger::error("Invalid terrain manager passed to PointCloudRenderer");
        return false;
    }

    if (!createShaders()) {
        Logger::error("Failed to create point cloud shaders");
        return false;
    }

    if (!setupVertexArrays()) {
        Logger::error("Failed to setup vertex arrays");
        return false;
    }

    // Reserve initial point buffer
    m_points.reserve(m_maxBufferSize);
    m_renderPoints.reserve(m_maxBufferSize);

    // Initialize rover visibility (all visible by default)
    for (int i = 1; i <= RoverConfig::MAX_ROVERS; i++) {
        m_roverVisibility[i] = true;
        m_roverColors[i] = getDefaultRoverColor(i);
    }

    m_initialized = true;
    Logger::info("PointCloudRenderer initialized successfully");
    return true;
}

void PointCloudRenderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clean up OpenGL resources
    if (m_pointVAO != 0) {
        glDeleteVertexArrays(1, &m_pointVAO);
        glDeleteBuffers(1, &m_pointVBO);
    }

    // Clear data
    {
        std::lock_guard<std::mutex> lock(m_pointsMutex);
        m_points.clear();
        m_renderPoints.clear();
    }

    m_roverVisibility.clear();
    m_roverColors.clear();
    m_initialized = false;

    Logger::info("PointCloudRenderer shutdown complete");
}

void PointCloudRenderer::update(const Camera& camera, float deltaTime) {
    if (!m_initialized) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    updatePointData(camera, deltaTime);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    m_lastUpdateTime += deltaTime;
}

void PointCloudRenderer::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                               const glm::vec3& cameraPosition) {
    if (!m_initialized || !m_pointShader) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    if (m_renderPoints.empty()) {
        m_stats.renderTime = 0.0;
        m_stats.renderedPoints = 0;
        return;
    }

    // Use point cloud shader
    m_pointShader->use();
    m_pointShader->setUniform("view", viewMatrix);
    m_pointShader->setUniform("projection", projectionMatrix);
    m_pointShader->setUniform("cameraPosition", cameraPosition);
    m_pointShader->setUniform("pointSizeMultiplier", m_config.pointSizeMultiplier);
    m_pointShader->setUniform("enableSizeAttenuation", m_config.enableSizeAttenuation);
    m_pointShader->setUniform("currentTime", static_cast<float>(m_lastUpdateTime));
    m_pointShader->setUniform("fadeTime", m_config.fadeTime);
    m_pointShader->setUniform("renderMode", static_cast<int>(m_config.renderMode));

    // Bind VAO and render points
    glBindVertexArray(m_pointVAO);
    
    // Enable point size programs for size attenuation
    if (m_config.enableSizeAttenuation) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_renderPoints.size()));

    if (m_config.enableSizeAttenuation) {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }

    glBindVertexArray(0);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.renderedPoints = m_renderPoints.size();

    updateStats();
}

void PointCloudRenderer::setConfig(const PointCloudConfig& config) {
    m_config = config;
    m_dataChanged = true;
}

void PointCloudRenderer::setRoverVisible(int roverId, bool visible) {
    if (roverId == 0) {
        // Set visibility for all rovers
        for (auto& pair : m_roverVisibility) {
            pair.second = visible;
        }
    } else {
        m_roverVisibility[roverId] = visible;
    }
    m_dataChanged = true;
}

bool PointCloudRenderer::isRoverVisible(int roverId) const {
    auto it = m_roverVisibility.find(roverId);
    return it != m_roverVisibility.end() ? it->second : false;
}

void PointCloudRenderer::setRoverColor(int roverId, const glm::vec4& color) {
    m_roverColors[roverId] = color;
    m_dataChanged = true;
}

glm::vec4 PointCloudRenderer::getRoverColor(int roverId) const {
    auto it = m_roverColors.find(roverId);
    return it != m_roverColors.end() ? it->second : m_config.defaultColor;
}

void PointCloudRenderer::clearAll() {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    m_points.clear();
    m_renderPoints.clear();
    m_dataChanged = true;
}

void PointCloudRenderer::clearRover(int roverId) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    m_points.erase(std::remove_if(m_points.begin(), m_points.end(),
                   [roverId](const GPUPoint& point) { return point.roverId == roverId; }),
                   m_points.end());
    
    m_renderPoints.erase(std::remove_if(m_renderPoints.begin(), m_renderPoints.end(),
                        [roverId](const GPUPoint& point) { return point.roverId == roverId; }),
                        m_renderPoints.end());
    
    m_dataChanged = true;
}

PointCloudStats PointCloudRenderer::getStats() const {
    return m_stats;
}

size_t PointCloudRenderer::getGPUMemoryUsage() const {
    return calculateGPUMemoryUsage();
}

void PointCloudRenderer::forceUpdate() {
    m_dataChanged = true;
}

std::string PointCloudRenderer::getDebugInfo() const {
    std::stringstream ss;
    ss << "PointCloudRenderer Debug Info:\n";
    ss << "Total Points: " << m_stats.totalPoints << "\n";
    ss << "Rendered Points: " << m_stats.renderedPoints << "\n";
    ss << "Culled Points: " << m_stats.culledPoints << "\n";
    ss << "LOD Skipped: " << m_stats.lodSkippedPoints << "\n";
    ss << "Update Time: " << m_stats.updateTime << "ms\n";
    ss << "Render Time: " << m_stats.renderTime << "ms\n";
    ss << "GPU Memory: " << (m_stats.gpuMemoryUsage / (1024 * 1024)) << "MB\n";
    ss << "Active Rovers: " << m_stats.activeRovers << "\n";
    ss << "Render Mode: " << static_cast<int>(m_config.renderMode) << "\n";
    ss << "LOD Enabled: " << (m_config.enableLOD ? "Yes" : "No") << "\n";
    ss << "Culling Enabled: " << (m_config.enableCulling ? "Yes" : "No") << "\n";
    return ss.str();
}

// Private methods
bool PointCloudRenderer::createShaders() {
    std::string vertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPosition;
        layout (location = 1) in float aIntensity;
        layout (location = 2) in vec4 aColor;
        layout (location = 3) in float aSize;
        layout (location = 4) in float aTimestamp;
        layout (location = 5) in int aRoverId;
        
        uniform mat4 view;
        uniform mat4 projection;
        uniform vec3 cameraPosition;
        uniform float pointSizeMultiplier;
        uniform bool enableSizeAttenuation;
        uniform float currentTime;
        uniform float fadeTime;
        uniform int renderMode;
        
        out vec4 vertexColor;
        out float vertexAlpha;
        
        vec4 getRoverColor(int roverId) {
            if (roverId == 1) return vec4(1.0, 0.2, 0.2, 1.0);      // Red
            else if (roverId == 2) return vec4(0.2, 1.0, 0.2, 1.0); // Green
            else if (roverId == 3) return vec4(0.2, 0.2, 1.0, 1.0); // Blue
            else if (roverId == 4) return vec4(1.0, 1.0, 0.2, 1.0); // Yellow
            else if (roverId == 5) return vec4(1.0, 0.2, 1.0, 1.0); // Magenta
            else return vec4(1.0, 1.0, 1.0, 1.0);                   // White
        }
        
        vec4 getHeightColor(float height) {
            float normalizedHeight = clamp((height + 10.0) / 20.0, 0.0, 1.0);
            return vec4(normalizedHeight, 0.5, 1.0 - normalizedHeight, 1.0);
        }
        
        vec4 getIntensityColor(float intensity) {
            return vec4(intensity, intensity, intensity, 1.0);
        }
        
        vec4 getTimestampColor(float timestamp, float currentTime) {
            float age = currentTime - timestamp;
            float normalizedAge = clamp(age / 30.0, 0.0, 1.0);
            return vec4(1.0 - normalizedAge, normalizedAge, 0.5, 1.0);
        }
        
        void main() {
            gl_Position = projection * view * vec4(aPosition, 1.0);
            
            // Calculate point color based on render mode
            if (renderMode == 0) {          // SOLID
                vertexColor = aColor;
            } else if (renderMode == 1) {   // INTENSITY
                vertexColor = getIntensityColor(aIntensity);
            } else if (renderMode == 2) {   // HEIGHT
                vertexColor = getHeightColor(aPosition.z);
            } else if (renderMode == 3) {   // ROVER_ID
                vertexColor = getRoverColor(aRoverId);
            } else if (renderMode == 4) {   // TIMESTAMP
                vertexColor = getTimestampColor(aTimestamp, currentTime);
            } else {
                vertexColor = aColor;
            }
            
            // Calculate fade alpha
            if (fadeTime > 0.0) {
                float age = currentTime - aTimestamp;
                vertexAlpha = 1.0 - clamp(age / fadeTime, 0.0, 1.0);
            } else {
                vertexAlpha = 1.0;
            }
            
            // Calculate point size
            float distance = length(cameraPosition - aPosition);
            float pointSize = aSize * pointSizeMultiplier;
            
            if (enableSizeAttenuation) {
                pointSize = pointSize * (100.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance));
                pointSize = clamp(pointSize, 1.0, 10.0);
            }
            
            gl_PointSize = pointSize;
        }
    )";
    
    std::string fragmentShader = R"(
        #version 330 core
        in vec4 vertexColor;
        in float vertexAlpha;
        
        out vec4 FragColor;
        
        void main() {
            // Create circular points
            vec2 coord = gl_PointCoord - vec2(0.5);
            float distance = length(coord);
            
            if (distance > 0.5) {
                discard;
            }
            
            // Smooth edges
            float alpha = 1.0 - smoothstep(0.3, 0.5, distance);
            alpha *= vertexAlpha;
            
            FragColor = vec4(vertexColor.rgb, alpha);
        }
    )";
    
    m_pointShader = m_renderer->createShaderProgram(vertexShader, fragmentShader);
    if (!m_pointShader) {
        Logger::error("Failed to create point cloud shader program");
        return false;
    }
    
    m_renderer->cacheShaderProgram("pointcloud", m_pointShader);
    return true;
}

bool PointCloudRenderer::setupVertexArrays() {
    // Generate VAO and VBO
    glGenVertexArrays(1, &m_pointVAO);
    glGenBuffers(1, &m_pointVBO);
    
    glBindVertexArray(m_pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_pointVBO);
    
    // Setup vertex attributes
    // Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GPUPoint), 
                         (void*)offsetof(GPUPoint, position));
    glEnableVertexAttribArray(0);
    
    // Intensity (location 1)
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GPUPoint), 
                         (void*)offsetof(GPUPoint, intensity));
    glEnableVertexAttribArray(1);
    
    // Color (location 2)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GPUPoint), 
                         (void*)offsetof(GPUPoint, color));
    glEnableVertexAttribArray(2);
    
    // Size (location 3)
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GPUPoint), 
                         (void*)offsetof(GPUPoint, size));
    glEnableVertexAttribArray(3);
    
    // Timestamp (location 4)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GPUPoint), 
                         (void*)offsetof(GPUPoint, timestamp));
    glEnableVertexAttribArray(4);
    
    // Rover ID (location 5)
    glVertexAttribIPointer(5, 1, GL_INT, sizeof(GPUPoint), 
                          (void*)offsetof(GPUPoint, roverId));
    glEnableVertexAttribArray(5);
    
    glBindVertexArray(0);
    return true;
}

void PointCloudRenderer::updatePointData(const Camera& camera, float deltaTime) {
    if (!m_terrainManager) {
        return;
    }

    // Check if we need to update data
    bool needsUpdate = m_dataChanged.exchange(false);
    
    if (!needsUpdate) {
        // Check if terrain manager has new data
        static size_t lastPointCount = 0;
        size_t currentPointCount = m_terrainManager->getPointCount();
        if (currentPointCount != lastPointCount) {
            needsUpdate = true;
            lastPointCount = currentPointCount;
        }
    }

    if (!needsUpdate) {
        return;
    }

    updateFromTerrainManager();
    
    // Apply frustum culling if enabled
    if (m_config.enableCulling) {
        Frustum frustum = camera.getViewFrustum(m_renderer->getAspectRatio());
        applyCulling(frustum);
        m_lastFrustum = frustum;
        m_frustumValid = true;
    } else {
        std::lock_guard<std::mutex> lock(m_pointsMutex);
        m_renderPoints = m_points;
    }
    
    // Apply LOD if enabled
    if (m_config.enableLOD) {
        applyLOD(camera.getPosition());
        m_lastCameraPosition = camera.getPosition();
    }
    
    // Apply colors and sizes
    applyColorAndSize(static_cast<float>(m_lastUpdateTime));
    
    // Upload to GPU
    uploadToGPU();
}

void PointCloudRenderer::updateFromTerrainManager() {
    if (!m_terrainManager) {
        return;
    }

    std::vector<TerrainPoint> terrainPoints;
    
    // Get points from terrain manager with optional time filtering
    if (m_config.showOnlyRecent) {
        double currentTime = m_lastUpdateTime;
        double cutoffTime = currentTime - m_config.recentTimeWindow;
        
        // Query all points and filter by time
        m_terrainManager->getRenderPoints(terrainPoints);
        
        terrainPoints.erase(std::remove_if(terrainPoints.begin(), terrainPoints.end(),
                           [cutoffTime](const TerrainPoint& point) {
                               return point.timestamp < cutoffTime;
                           }), terrainPoints.end());
    } else {
        m_terrainManager->getRenderPoints(terrainPoints);
    }

    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    // Convert terrain points to GPU points
    m_points.clear();
    m_points.reserve(terrainPoints.size());
    
    float currentTime = static_cast<float>(m_lastUpdateTime);
    
    for (const TerrainPoint& terrainPoint : terrainPoints) {
        // Check rover visibility
        if (!isRoverVisible(terrainPoint.roverId)) {
            continue;
        }
        
        m_points.emplace_back(terrainPoint, currentTime);
    }
    
    m_stats.totalPoints = m_points.size();
}

void PointCloudRenderer::applyCulling(const Frustum& frustum) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    m_renderPoints.clear();
    m_renderPoints.reserve(m_points.size());
    
    size_t culledCount = 0;
    
    for (const GPUPoint& point : m_points) {
        if (isPointInFrustum(point.position, frustum)) {
            m_renderPoints.push_back(point);
        } else {
            culledCount++;
        }
    }
    
    m_stats.culledPoints = culledCount;
}

void PointCloudRenderer::applyLOD(const glm::vec3& cameraPosition) {
    if (m_renderPoints.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    size_t originalCount = m_renderPoints.size();
    size_t skipCount = 0;
    
    // Sort points by distance if needed for LOD
    std::vector<GPUPoint> lodPoints;
    lodPoints.reserve(m_renderPoints.size());
    
    for (const GPUPoint& point : m_renderPoints) {
        float distance = glm::length(cameraPosition - point.position);
        
        // Determine skip probability based on distance
        float skipProbability = 0.0f;
        if (distance > m_config.lod.farDistance) {
            skipProbability = m_config.lod.skipRatio;
        } else if (distance > m_config.lod.nearDistance) {
            float t = (distance - m_config.lod.nearDistance) / 
                     (m_config.lod.farDistance - m_config.lod.nearDistance);
            skipProbability = t * m_config.lod.skipRatio;
        }
        
        // Simple deterministic skipping based on position hash
        if (skipProbability > 0.0f) {
            uint32_t hash = static_cast<uint32_t>(point.position.x * 1000) ^
                           static_cast<uint32_t>(point.position.y * 1000) ^
                           static_cast<uint32_t>(point.position.z * 1000);
            float random = static_cast<float>(hash % 1000) / 1000.0f;
            
            if (random < skipProbability) {
                skipCount++;
                continue;
            }
        }
        
        lodPoints.push_back(point);
    }
    
    m_renderPoints = std::move(lodPoints);
    m_stats.lodSkippedPoints = skipCount;
}

void PointCloudRenderer::applyColorAndSize(float currentTime) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    for (GPUPoint& point : m_renderPoints) {
        // Apply color based on render mode
        point.color = calculatePointColor(point, currentTime);
        
        // Apply size based on distance (done in shader for performance)
        point.size = m_config.pointSizeMultiplier;
    }
}

void PointCloudRenderer::uploadToGPU() {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    
    if (m_renderPoints.empty()) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_pointVBO);
    
    size_t dataSize = m_renderPoints.size() * sizeof(GPUPoint);
    
    // Resize buffer if needed
    if (dataSize > m_bufferSize) {
        glBufferData(GL_ARRAY_BUFFER, dataSize, m_renderPoints.data(), GL_DYNAMIC_DRAW);
        m_bufferSize = dataSize;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, m_renderPoints.data());
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

glm::vec4 PointCloudRenderer::calculatePointColor(const GPUPoint& point, float currentTime) const {
    switch (m_config.renderMode) {
        case PointRenderMode::SOLID:
            return point.color;
            
        case PointRenderMode::INTENSITY:
            return getColorByIntensity(point.intensity);
            
        case PointRenderMode::HEIGHT:
            return getColorByHeight(point.position.z);
            
        case PointRenderMode::ROVER_ID:
            return getColorByRoverId(point.roverId);
            
        case PointRenderMode::TIMESTAMP:
            return getColorByTimestamp(point.timestamp, currentTime);
            
        default:
            return m_config.defaultColor;
    }
}

float PointCloudRenderer::calculatePointSize(const GPUPoint& point, const glm::vec3& cameraPosition) const {
    if (!m_config.enableSizeAttenuation) {
        return m_config.pointSizeMultiplier;
    }
    
    float distance = glm::length(cameraPosition - point.position);
    float attenuation = calculateDistanceAttenuation(distance);
    float size = m_config.pointSizeMultiplier * attenuation;
    
    return glm::clamp(size, m_config.minPointSize, m_config.maxPointSize);
}

float PointCloudRenderer::calculateDistanceAttenuation(float distance) const {
    // Quadratic attenuation formula
    float constant = 1.0f;
    float linear = 0.045f;
    float quadratic = 0.0075f;
    
    return 1.0f / (constant + linear * distance + quadratic * distance * distance);
}

bool PointCloudRenderer::shouldRenderPoint(const GPUPoint& point, float currentTime) const {
    // Check fade time
    if (m_config.fadeTime > 0.0f) {
        float age = currentTime - point.timestamp;
        if (age > m_config.fadeTime) {
            return false;
        }
    }
    
    // Check rover visibility
    return isRoverVisible(point.roverId);
}

bool PointCloudRenderer::isPointInFrustum(const glm::vec3& position, const Frustum& frustum) const {
    return frustum.containsPoint(position);
}

void PointCloudRenderer::updateStats() {
    m_stats.gpuMemoryUsage = calculateGPUMemoryUsage();
    
    // Count active rovers
    std::set<int> activeRovers;
    {
        std::lock_guard<std::mutex> lock(m_pointsMutex);
        for (const GPUPoint& point : m_renderPoints) {
            activeRovers.insert(point.roverId);
        }
    }
    m_stats.activeRovers = static_cast<int>(activeRovers.size());
}

size_t PointCloudRenderer::calculateGPUMemoryUsage() const {
    return m_bufferSize + sizeof(GPUPoint) * m_points.capacity();
}

// Color mapping functions
glm::vec4 PointCloudRenderer::getColorByRoverId(int roverId) const {
    return getRoverColor(roverId);
}

glm::vec4 PointCloudRenderer::getColorByHeight(float height) const {
    // Map height to a color gradient (blue to red)
    float normalizedHeight = glm::clamp((height + 10.0f) / 20.0f, 0.0f, 1.0f);
    return glm::vec4(normalizedHeight, 0.5f, 1.0f - normalizedHeight, 1.0f);
}

glm::vec4 PointCloudRenderer::getColorByIntensity(float intensity) const {
    return glm::vec4(intensity, intensity, intensity, 1.0f);
}

glm::vec4 PointCloudRenderer::getColorByTimestamp(float timestamp, float currentTime) const {
    float age = currentTime - timestamp;
    float normalizedAge = glm::clamp(age / 30.0f, 0.0f, 1.0f);
    return glm::vec4(1.0f - normalizedAge, normalizedAge, 0.5f, 1.0f);
}

glm::vec4 PointCloudRenderer::getDefaultRoverColor(int roverId) {
    // Same colors as RoverRenderer
    static const glm::vec4 colors[5] = {
        glm::vec4(1.0f, 0.2f, 0.2f, 1.0f),  // Red
        glm::vec4(0.2f, 1.0f, 0.2f, 1.0f),  // Green
        glm::vec4(0.2f, 0.2f, 1.0f, 1.0f),  // Blue
        glm::vec4(1.0f, 1.0f, 0.2f, 1.0f),  // Yellow
        glm::vec4(1.0f, 0.2f, 1.0f, 1.0f)   // Magenta
    };
    
    if (roverId >= 1 && roverId <= 5) {
        return colors[roverId - 1];
    }
    
    return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White for unknown rovers
}