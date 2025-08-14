#include "RoverRenderer.h"
#include "Constants.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>
#include <cmath>

RoverRenderer::RoverRenderer()
    : m_initialized(false)
    , m_cubeVAO(0), m_cubeVBO(0), m_cubeEBO(0)
    , m_cylinderVAO(0), m_cylinderVBO(0), m_cylinderEBO(0)
    , m_trailVAO(0), m_trailVBO(0)
    , m_lastUpdateTime(0.0)
    , m_selectedRoverId(0) {
}

RoverRenderer::~RoverRenderer() {
    shutdown();
}

bool RoverRenderer::initialize(std::shared_ptr<Renderer> renderer) {
    if (m_initialized) {
        Logger::warning("RoverRenderer already initialized");
        return true;
    }

    m_renderer = renderer;
    if (!m_renderer) {
        Logger::error("Invalid renderer passed to RoverRenderer");
        return false;
    }

    if (!createGeometry()) {
        Logger::error("Failed to create rover geometry");
        return false;
    }

    if (!createShaders()) {
        Logger::error("Failed to create rover shaders");
        return false;
    }

    setupVertexArrays();

    m_initialized = true;
    Logger::info("RoverRenderer initialized successfully");
    return true;
}

void RoverRenderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clean up OpenGL resources
    if (m_cubeVAO != 0) {
        glDeleteVertexArrays(1, &m_cubeVAO);
        glDeleteBuffers(1, &m_cubeVBO);
        glDeleteBuffers(1, &m_cubeEBO);
    }

    if (m_cylinderVAO != 0) {
        glDeleteVertexArrays(1, &m_cylinderVAO);
        glDeleteBuffers(1, &m_cylinderVBO);
        glDeleteBuffers(1, &m_cylinderEBO);
    }

    if (m_trailVAO != 0) {
        glDeleteVertexArrays(1, &m_trailVAO);
        glDeleteBuffers(1, &m_trailVBO);
    }

    m_rovers.clear();
    m_trails.clear();
    m_initialized = false;

    Logger::info("RoverRenderer shutdown complete");
}

void RoverRenderer::updateRoverPose(int roverId, const PosePacket& pose) {
    if (!m_initialized) {
        return;
    }

    RoverVisual& rover = m_rovers[roverId];
    rover.roverId = roverId;
    rover.position = glm::vec3(pose.posX, pose.posY, pose.posZ);
    rover.rotation = glm::vec3(pose.rotXdeg, pose.rotYdeg, pose.rotZdeg);
    rover.lastUpdateTime = pose.timestamp;
    
    // Set default color if this is a new rover
    if (rover.color == glm::vec4(1.0f)) {
        rover.color = getDefaultRoverColor(roverId);
    }

    updateRoverMatrix(rover);

    // Add trail point if trails are enabled
    if (m_config.showTrails) {
        addTrailPoint(roverId, rover.position, pose.timestamp);
    }
}

void RoverRenderer::update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    m_lastUpdateTime += deltaTime;
    updateTrails(deltaTime);
}

void RoverRenderer::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, 
                          const glm::vec3& cameraPosition) {
    if (!m_initialized || !m_renderer) {
        return;
    }

    // Render rovers
    renderRovers(viewMatrix, projectionMatrix);

    // Render trails
    if (m_config.showTrails) {
        renderTrails(viewMatrix, projectionMatrix);
    }

    // Render labels
    if (m_config.showLabels) {
        renderLabels(viewMatrix, projectionMatrix);
    }
}

void RoverRenderer::setRoverVisible(int roverId, bool visible) {
    if (roverId == 0) {
        // Set visibility for all rovers
        for (auto& pair : m_rovers) {
            pair.second.visible = visible;
        }
    } else {
        auto it = m_rovers.find(roverId);
        if (it != m_rovers.end()) {
            it->second.visible = visible;
        }
    }
}

bool RoverRenderer::isRoverVisible(int roverId) const {
    auto it = m_rovers.find(roverId);
    return it != m_rovers.end() ? it->second.visible : false;
}

void RoverRenderer::setRoverSelected(int roverId, bool selected) {
    if (selected) {
        // Clear previous selection
        for (auto& pair : m_rovers) {
            pair.second.selected = false;
        }
        
        auto it = m_rovers.find(roverId);
        if (it != m_rovers.end()) {
            it->second.selected = true;
            m_selectedRoverId = roverId;
        }
    } else {
        auto it = m_rovers.find(roverId);
        if (it != m_rovers.end()) {
            it->second.selected = false;
            if (m_selectedRoverId == roverId) {
                m_selectedRoverId = 0;
            }
        }
    }
}

int RoverRenderer::getSelectedRover() const {
    return m_selectedRoverId;
}

void RoverRenderer::clearSelection() {
    for (auto& pair : m_rovers) {
        pair.second.selected = false;
    }
    m_selectedRoverId = 0;
}

void RoverRenderer::setRoverColor(int roverId, const glm::vec4& color) {
    auto it = m_rovers.find(roverId);
    if (it != m_rovers.end()) {
        it->second.color = color;
    }
}

glm::vec4 RoverRenderer::getRoverColor(int roverId) const {
    auto it = m_rovers.find(roverId);
    return it != m_rovers.end() ? it->second.color : glm::vec4(1.0f);
}

glm::vec4 RoverRenderer::getDefaultRoverColor(int roverId) {
    // Define 5 distinct colors for rovers 1-5
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
    
    // For rovers beyond 5, generate a color based on ID
    float hue = static_cast<float>(roverId * 137.5f); // Golden angle for good distribution
    hue = fmod(hue, 360.0f);
    
    // Convert HSV to RGB (simplified)
    float c = 1.0f; // Full saturation
    float x = 1.0f - abs(fmod(hue / 60.0f, 2.0f) - 1.0f);
    float m = 0.0f; // Full brightness
    
    glm::vec3 rgb;
    if (hue < 60.0f) rgb = glm::vec3(c, x, 0);
    else if (hue < 120.0f) rgb = glm::vec3(x, c, 0);
    else if (hue < 180.0f) rgb = glm::vec3(0, c, x);
    else if (hue < 240.0f) rgb = glm::vec3(0, x, c);
    else if (hue < 300.0f) rgb = glm::vec3(x, 0, c);
    else rgb = glm::vec3(c, 0, x);
    
    return glm::vec4(rgb + m, 1.0f);
}

void RoverRenderer::setRenderConfig(const RoverRenderConfig& config) {
    m_config = config;
}

void RoverRenderer::clearAllTrails() {
    m_trails.clear();
}

void RoverRenderer::clearRoverTrail(int roverId) {
    auto it = m_trails.find(roverId);
    if (it != m_trails.end()) {
        it->second.clear();
    }
}

std::vector<int> RoverRenderer::getRoverIds() const {
    std::vector<int> ids;
    ids.reserve(m_rovers.size());
    for (const auto& pair : m_rovers) {
        ids.push_back(pair.first);
    }
    return ids;
}

glm::vec3 RoverRenderer::getRoverPosition(int roverId) const {
    auto it = m_rovers.find(roverId);
    return it != m_rovers.end() ? it->second.position : glm::vec3(0.0f);
}

glm::vec3 RoverRenderer::getRoverRotation(int roverId) const {
    auto it = m_rovers.find(roverId);
    return it != m_rovers.end() ? it->second.rotation : glm::vec3(0.0f);
}

bool RoverRenderer::hasRover(int roverId) const {
    return m_rovers.find(roverId) != m_rovers.end();
}

void RoverRenderer::removeRover(int roverId) {
    m_rovers.erase(roverId);
    m_trails.erase(roverId);
    if (m_selectedRoverId == roverId) {
        m_selectedRoverId = 0;
    }
}

std::string RoverRenderer::getDebugInfo() const {
    std::stringstream ss;
    ss << "RoverRenderer Debug Info:\n";
    ss << "Active Rovers: " << m_rovers.size() << "\n";
    ss << "Selected Rover: " << m_selectedRoverId << "\n";
    ss << "Trails Enabled: " << (m_config.showTrails ? "Yes" : "No") << "\n";
    ss << "Labels Enabled: " << (m_config.showLabels ? "Yes" : "No") << "\n";
    
    for (const auto& pair : m_rovers) {
        const RoverVisual& rover = pair.second;
        ss << "Rover " << rover.roverId << ": ";
        ss << "Pos(" << rover.position.x << ", " << rover.position.y << ", " << rover.position.z << ") ";
        ss << "Visible: " << (rover.visible ? "Y" : "N") << " ";
        ss << "Selected: " << (rover.selected ? "Y" : "N") << "\n";
    }
    
    return ss.str();
}

// Private methods
bool RoverRenderer::createGeometry() {
    generateCubeGeometry();
    generateCylinderGeometry();
    return true;
}

bool RoverRenderer::createShaders() {
    // Create rover shader
    std::string vertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoord;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform vec3 lightPos;
        
        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoord;
        out vec3 LightPos;
        
        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            TexCoord = aTexCoord;
            LightPos = lightPos;
            
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";
    
    std::string fragmentShader = R"(
        #version 330 core
        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoord;
        in vec3 LightPos;
        
        uniform vec4 objectColor;
        uniform vec3 lightColor;
        uniform vec3 viewPos;
        uniform bool selected;
        
        out vec4 FragColor;
        
        void main() {
            // Ambient
            float ambientStrength = 0.3;
            vec3 ambient = ambientStrength * lightColor;
            
            // Diffuse
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(LightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;
            
            // Specular
            float specularStrength = 0.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
            vec3 specular = specularStrength * spec * lightColor;
            
            vec3 result = (ambient + diffuse + specular) * objectColor.rgb;
            
            // Selection highlight
            if (selected) {
                result = mix(result, vec3(1.0, 1.0, 0.0), 0.3);
            }
            
            FragColor = vec4(result, objectColor.a);
        }
    )";
    
    m_roverShader = m_renderer->createShaderProgram(vertexShader, fragmentShader);
    if (!m_roverShader) {
        Logger::error("Failed to create rover shader program");
        return false;
    }
    
    m_renderer->cacheShaderProgram("rover", m_roverShader);
    
    // Create trail shader
    std::string trailVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in float aAlpha;
        
        uniform mat4 view;
        uniform mat4 projection;
        uniform vec4 trailColor;
        
        out vec4 vertexColor;
        
        void main() {
            vertexColor = vec4(trailColor.rgb, trailColor.a * aAlpha);
            gl_Position = projection * view * vec4(aPos, 1.0);
        }
    )";
    
    std::string trailFragmentShader = R"(
        #version 330 core
        in vec4 vertexColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vertexColor;
        }
    )";
    
    m_trailShader = m_renderer->createShaderProgram(trailVertexShader, trailFragmentShader);
    if (!m_trailShader) {
        Logger::error("Failed to create trail shader program");
        return false;
    }
    
    m_renderer->cacheShaderProgram("trail", m_trailShader);
    
    return true;
}

void RoverRenderer::generateCubeGeometry() {
    // Cube vertices with normals and texture coordinates
    m_cubeGeometry.vertices = {
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        
        // Left face
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        
        // Right face
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
        
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f
    };
    
    m_cubeGeometry.indices = {
        0,  1,  2,   2,  3,  0,   // Front
        4,  5,  6,   6,  7,  4,   // Back
        8,  9,  10,  10, 11, 8,   // Left
        12, 13, 14,  14, 15, 12,  // Right
        16, 17, 18,  18, 19, 16,  // Top
        20, 21, 22,  22, 23, 20   // Bottom
    };
}

void RoverRenderer::generateCylinderGeometry(int segments) {
    m_cylinderGeometry.vertices.clear();
    m_cylinderGeometry.indices.clear();
    
    float radius = 0.5f;
    float height = 1.0f;
    
    // Generate vertices for top and bottom circles
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        
        // Top circle
        addCylinderVertex(m_cylinderGeometry.vertices, 
                         glm::vec3(x, height/2, z), 
                         glm::vec3(0, 1, 0), 
                         glm::vec2(0.5f + x, 0.5f + z));
        
        // Bottom circle
        addCylinderVertex(m_cylinderGeometry.vertices, 
                         glm::vec3(x, -height/2, z), 
                         glm::vec3(0, -1, 0), 
                         glm::vec2(0.5f + x, 0.5f + z));
        
        // Side vertices
        addCylinderVertex(m_cylinderGeometry.vertices, 
                         glm::vec3(x, height/2, z), 
                         glm::vec3(x/radius, 0, z/radius), 
                         glm::vec2((float)i/segments, 1.0f));
        
        addCylinderVertex(m_cylinderGeometry.vertices, 
                         glm::vec3(x, -height/2, z), 
                         glm::vec3(x/radius, 0, z/radius), 
                         glm::vec2((float)i/segments, 0.0f));
    }
    
    // Generate indices
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        
        // Side faces
        int base = i * 4 + 2;
        m_cylinderGeometry.indices.push_back(base);
        m_cylinderGeometry.indices.push_back(base + 1);
        m_cylinderGeometry.indices.push_back(base + 4);
        
        m_cylinderGeometry.indices.push_back(base + 1);
        m_cylinderGeometry.indices.push_back(base + 5);
        m_cylinderGeometry.indices.push_back(base + 4);
    }
}

void RoverRenderer::addCylinderVertex(std::vector<float>& vertices, const glm::vec3& position, 
                                     const glm::vec3& normal, const glm::vec2& texCoord) {
    vertices.push_back(position.x);
    vertices.push_back(position.y);
    vertices.push_back(position.z);
    vertices.push_back(normal.x);
    vertices.push_back(normal.y);
    vertices.push_back(normal.z);
    vertices.push_back(texCoord.x);
    vertices.push_back(texCoord.y);
}

void RoverRenderer::setupVertexArrays() {
    // Setup cube VAO
    glGenVertexArrays(1, &m_cubeVAO);
    glGenBuffers(1, &m_cubeVBO);
    glGenBuffers(1, &m_cubeEBO);
    
    glBindVertexArray(m_cubeVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, m_cubeGeometry.vertices.size() * sizeof(float), 
                 m_cubeGeometry.vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_cubeGeometry.indices.size() * sizeof(unsigned int), 
                 m_cubeGeometry.indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Setup cylinder VAO
    glGenVertexArrays(1, &m_cylinderVAO);
    glGenBuffers(1, &m_cylinderVBO);
    glGenBuffers(1, &m_cylinderEBO);
    
    glBindVertexArray(m_cylinderVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderVBO);
    glBufferData(GL_ARRAY_BUFFER, m_cylinderGeometry.vertices.size() * sizeof(float), 
                 m_cylinderGeometry.vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cylinderEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_cylinderGeometry.indices.size() * sizeof(unsigned int), 
                 m_cylinderGeometry.indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Setup trail VAO
    glGenVertexArrays(1, &m_trailVAO);
    glGenBuffers(1, &m_trailVBO);
    
    glBindVertexArray(m_trailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_trailVBO);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Alpha attribute
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void RoverRenderer::renderRovers(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!m_roverShader) {
        return;
    }

    m_roverShader->use();
    m_roverShader->setUniform("view", viewMatrix);
    m_roverShader->setUniform("projection", projectionMatrix);
    m_roverShader->setUniform("lightPos", glm::vec3(10.0f, 10.0f, 10.0f));
    m_roverShader->setUniform("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    m_roverShader->setUniform("viewPos", glm::vec3(0.0f, 0.0f, 0.0f)); // TODO: Get from camera

    // Choose geometry based on config
    GLuint vao = (m_config.geometry == RoverGeometry::CUBE) ? m_cubeVAO : m_cylinderVAO;
    size_t indexCount = (m_config.geometry == RoverGeometry::CUBE) ? 
                        m_cubeGeometry.indices.size() : m_cylinderGeometry.indices.size();

    glBindVertexArray(vao);

    for (const auto& pair : m_rovers) {
        const RoverVisual& rover = pair.second;
        
        if (!rover.visible) {
            continue;
        }

        m_roverShader->setUniform("model", rover.modelMatrix);
        m_roverShader->setUniform("objectColor", rover.color);
        m_roverShader->setUniform("selected", rover.selected);

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}

void RoverRenderer::renderTrails(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!m_trailShader || m_trails.empty()) {
        return;
    }

    m_trailShader->use();
    m_trailShader->setUniform("view", viewMatrix);
    m_trailShader->setUniform("projection", projectionMatrix);

    glBindVertexArray(m_trailVAO);
    glLineWidth(2.0f);

    for (const auto& pair : m_trails) {
        int roverId = pair.first;
        const std::vector<TrailPoint>& trail = pair.second;
        
        if (trail.size() < 2) {
            continue;
        }

        // Check if rover is visible
        auto roverIt = m_rovers.find(roverId);
        if (roverIt == m_rovers.end() || !roverIt->second.visible) {
            continue;
        }

        // Prepare trail data
        std::vector<float> trailData;
        trailData.reserve(trail.size() * 4);
        
        for (const TrailPoint& point : trail) {
            trailData.push_back(point.position.x);
            trailData.push_back(point.position.y);
            trailData.push_back(point.position.z);
            trailData.push_back(point.alpha);
        }

        // Upload trail data
        glBindBuffer(GL_ARRAY_BUFFER, m_trailVBO);
        glBufferData(GL_ARRAY_BUFFER, trailData.size() * sizeof(float), 
                     trailData.data(), GL_DYNAMIC_DRAW);

        // Set trail color (based on rover color)
        glm::vec4 trailColor = roverIt->second.color;
        trailColor.a *= 0.7f; // Make trails semi-transparent
        m_trailShader->setUniform("trailColor", trailColor);

        // Draw trail as line strip
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(trail.size()));
    }

    glBindVertexArray(0);
}

void RoverRenderer::renderLabels(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    // Label rendering would require text rendering capability
    // This is a placeholder for future implementation
}

void RoverRenderer::updateRoverMatrix(RoverVisual& rover) {
    rover.modelMatrix = createModelMatrix(rover.position, rover.rotation, m_config.scale);
}

void RoverRenderer::updateTrails(float deltaTime) {
    double currentTime = m_lastUpdateTime;
    
    for (auto& pair : m_trails) {
        std::vector<TrailPoint>& trail = pair.second;
        
        // Update trail point alphas based on age
        for (TrailPoint& point : trail) {
            double age = currentTime - point.timestamp;
            if (age > 0 && m_config.trailLength > 0) {
                point.alpha = 1.0f - static_cast<float>(age) / m_config.trailLength;
                point.alpha = glm::max(0.0f, point.alpha);
            }
        }
        
        // Remove old trail points
        trail.erase(std::remove_if(trail.begin(), trail.end(),
                   [](const TrailPoint& point) { return point.alpha <= 0.0f; }),
                   trail.end());
        
        // Limit trail length
        trimTrail(trail);
    }
}

void RoverRenderer::addTrailPoint(int roverId, const glm::vec3& position, double timestamp) {
    std::vector<TrailPoint>& trail = m_trails[roverId];
    
    // Don't add point if it's too close to the last one
    if (!trail.empty()) {
        const TrailPoint& lastPoint = trail.back();
        float distance = glm::length(position - lastPoint.position);
        if (distance < 0.1f) { // Minimum distance threshold
            return;
        }
    }
    
    trail.emplace_back(position, timestamp);
    trimTrail(trail);
}

void RoverRenderer::trimTrail(std::vector<TrailPoint>& trail) {
    if (trail.size() > static_cast<size_t>(m_config.maxTrailPoints)) {
        trail.erase(trail.begin(), trail.end() - m_config.maxTrailPoints);
    }
}

glm::mat4 RoverRenderer::createModelMatrix(const glm::vec3& position, const glm::vec3& rotation, float scale) const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(scale));
    return model;
}