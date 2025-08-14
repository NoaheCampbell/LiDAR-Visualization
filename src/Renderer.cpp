#include "Renderer.h"
#include "Logger.h"
#include "Constants.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// ShaderProgram implementation
Renderer::ShaderProgram::~ShaderProgram() {
    if (programId != 0) {
        glDeleteProgram(programId);
    }
}

void Renderer::ShaderProgram::use() const {
    glUseProgram(programId);
}

void Renderer::ShaderProgram::setUniform(const std::string& name, float value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Renderer::ShaderProgram::setUniform(const std::string& name, int value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Renderer::ShaderProgram::setUniform(const std::string& name, const glm::vec3& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, 1, glm::value_ptr(value));
    }
}

void Renderer::ShaderProgram::setUniform(const std::string& name, const glm::vec4& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, 1, glm::value_ptr(value));
    }
}

void Renderer::ShaderProgram::setUniform(const std::string& name, const glm::mat4& value) {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}

GLint Renderer::ShaderProgram::getUniformLocation(const std::string& name) {
    auto it = uniformLocations.find(name);
    if (it != uniformLocations.end()) {
        return it->second;
    }
    
    GLint location = glGetUniformLocation(programId, name.c_str());
    uniformLocations[name] = location;
    return location;
}

// Renderer implementation
Renderer::Renderer() 
    : m_window(nullptr)
    , m_initialized(false)
    , m_fullscreen(false)
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_windowPosX(0)
    , m_windowPosY(0)
    , m_lastFrameTime(0.0)
    , m_frameTimeAccumulator(0.0)
    , m_frameCount(0)
    , m_fps(0.0) {
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int width, int height, const std::string& title, bool fullscreen) {
    if (m_initialized) {
        Logger::warning("Renderer already initialized");
        return true;
    }

    m_windowWidth = width;
    m_windowHeight = height;
    m_windowTitle = title;
    m_fullscreen = fullscreen;

    if (!initializeGLFW()) {
        Logger::error("Failed to initialize GLFW");
        return false;
    }

    if (!createWindow(width, height, title, fullscreen)) {
        Logger::error("Failed to create window");
        glfwTerminate();
        return false;
    }

    if (!initializeOpenGL()) {
        Logger::error("Failed to initialize OpenGL");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    setupOpenGLState();
    
    m_lastFrameTime = glfwGetTime();
    m_initialized = true;
    
    Logger::info("Renderer initialized successfully");
    Logger::info("OpenGL Version: " + getOpenGLVersion());
    Logger::info("Renderer: " + getRendererInfo());
    
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clear shader cache
    m_shaderCache.clear();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
    m_initialized = false;
    
    Logger::info("Renderer shutdown complete");
}

bool Renderer::shouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void Renderer::pollEvents() {
    glfwPollEvents();
}

void Renderer::swapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
        updateFPS();
    }
}

void Renderer::beginFrame(const glm::vec4& clearColor) {
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::endFrame() {
    // Flush any remaining OpenGL commands
    glFlush();
}

void Renderer::getWindowSize(int& width, int& height) const {
    if (m_window) {
        glfwGetWindowSize(m_window, &width, &height);
    } else {
        width = m_windowWidth;
        height = m_windowHeight;
    }
}

void Renderer::getFramebufferSize(int& width, int& height) const {
    if (m_window) {
        glfwGetFramebufferSize(m_window, &width, &height);
    } else {
        width = m_windowWidth;
        height = m_windowHeight;
    }
}

float Renderer::getAspectRatio() const {
    int width, height;
    getFramebufferSize(width, height);
    return height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
}

void Renderer::setWindowSize(int width, int height) {
    if (m_window && !m_fullscreen) {
        glfwSetWindowSize(m_window, width, height);
        m_windowWidth = width;
        m_windowHeight = height;
    }
}

bool Renderer::toggleFullscreen() {
    if (!m_window) {
        return false;
    }

    if (m_fullscreen) {
        // Switch to windowed mode
        glfwSetWindowMonitor(m_window, nullptr, m_windowPosX, m_windowPosY, 
                           m_windowWidth, m_windowHeight, GLFW_DONT_CARE);
        m_fullscreen = false;
    } else {
        // Store current window position and size
        glfwGetWindowPos(m_window, &m_windowPosX, &m_windowPosY);
        glfwGetWindowSize(m_window, &m_windowWidth, &m_windowHeight);
        
        // Switch to fullscreen mode
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
    }

    return m_fullscreen;
}

void Renderer::setWindowTitle(const std::string& title) {
    m_windowTitle = title;
    if (m_window) {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

void Renderer::setVSync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
}

void Renderer::setWindowIcon(const unsigned char* pixels, int width, int height) {
    if (m_window && pixels) {
        GLFWimage icon;
        icon.width = width;
        icon.height = height;
        icon.pixels = const_cast<unsigned char*>(pixels);
        glfwSetWindowIcon(m_window, 1, &icon);
    }
}

std::shared_ptr<Renderer::ShaderProgram> Renderer::createShaderProgram(
    const std::string& vertexSource,
    const std::string& fragmentSource,
    const std::string& geometrySource) {
    
    auto program = std::make_shared<ShaderProgram>();
    
    // Compile vertex shader
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        Logger::error("Failed to compile vertex shader");
        return nullptr;
    }

    // Compile fragment shader
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        Logger::error("Failed to compile fragment shader");
        glDeleteShader(vertexShader);
        return nullptr;
    }

    // Compile geometry shader if provided
    GLuint geometryShader = 0;
    if (!geometrySource.empty()) {
        geometryShader = compileShader(GL_GEOMETRY_SHADER, geometrySource);
        if (geometryShader == 0) {
            Logger::error("Failed to compile geometry shader");
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return nullptr;
        }
    }

    // Create and link program
    program->programId = glCreateProgram();
    glAttachShader(program->programId, vertexShader);
    glAttachShader(program->programId, fragmentShader);
    if (geometryShader != 0) {
        glAttachShader(program->programId, geometryShader);
    }

    glLinkProgram(program->programId);

    // Check link status
    GLint success;
    glGetProgramiv(program->programId, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program->programId, 512, nullptr, infoLog);
        Logger::error("Shader program linking failed: " + std::string(infoLog));
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        if (geometryShader != 0) {
            glDeleteShader(geometryShader);
        }
        return nullptr;
    }

    // Clean up shaders (they're now linked into the program)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader != 0) {
        glDeleteShader(geometryShader);
    }

    return program;
}

std::shared_ptr<Renderer::ShaderProgram> Renderer::loadShaderProgram(
    const std::string& vertexPath,
    const std::string& fragmentPath,
    const std::string& geometryPath) {
    
    std::string vertexSource = loadShaderFile(vertexPath);
    if (vertexSource.empty()) {
        Logger::error("Failed to load vertex shader: " + vertexPath);
        return nullptr;
    }

    std::string fragmentSource = loadShaderFile(fragmentPath);
    if (fragmentSource.empty()) {
        Logger::error("Failed to load fragment shader: " + fragmentPath);
        return nullptr;
    }

    std::string geometrySource;
    if (!geometryPath.empty()) {
        geometrySource = loadShaderFile(geometryPath);
        if (geometrySource.empty()) {
            Logger::error("Failed to load geometry shader: " + geometryPath);
            return nullptr;
        }
    }

    return createShaderProgram(vertexSource, fragmentSource, geometrySource);
}

std::shared_ptr<Renderer::ShaderProgram> Renderer::getShaderProgram(const std::string& name) const {
    auto it = m_shaderCache.find(name);
    return it != m_shaderCache.end() ? it->second : nullptr;
}

void Renderer::cacheShaderProgram(const std::string& name, std::shared_ptr<ShaderProgram> program) {
    m_shaderCache[name] = program;
}

bool Renderer::checkGLError(const std::string& operation) const {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::string message = "OpenGL error";
        if (!operation.empty()) {
            message += " in " + operation;
        }
        message += ": ";
        
        switch (error) {
            case GL_INVALID_ENUM: message += "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: message += "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: message += "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: message += "GL_OUT_OF_MEMORY"; break;
            default: message += "Unknown error " + std::to_string(error); break;
        }
        
        Logger::error(message);
        return false;
    }
    return true;
}

void Renderer::setWireframeMode(bool enabled) {
    glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
}

void Renderer::setDepthTesting(bool enabled) {
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::setFaceCulling(bool enabled) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::setPointSize(float size) {
    glPointSize(size);
}

void Renderer::setPointSizeAttenuation(bool enabled) {
    if (enabled) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    } else {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
}

std::string Renderer::getOpenGLVersion() const {
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return version ? std::string(version) : "Unknown";
}

std::string Renderer::getRendererInfo() const {
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer ? std::string(renderer) : "Unknown";
}

void Renderer::setKeyCallback(const KeyCallback& callback) {
    m_keyCallback = callback;
}

void Renderer::setMouseCallback(const MouseCallback& callback) {
    m_mouseCallback = callback;
}

void Renderer::setMouseButtonCallback(const MouseButtonCallback& callback) {
    m_mouseButtonCallback = callback;
}

void Renderer::setScrollCallback(const ScrollCallback& callback) {
    m_scrollCallback = callback;
}

void Renderer::setFramebufferSizeCallback(const FramebufferSizeCallback& callback) {
    m_framebufferSizeCallback = callback;
}

bool Renderer::captureScreenshot(const std::string& filename) const {
    if (!m_window) {
        return false;
    }

    int width, height;
    getFramebufferSize(width, height);
    
    std::vector<unsigned char> pixels(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    
    // Note: This is a simple implementation. A full implementation would
    // include proper image file writing (PNG, JPG, etc.)
    Logger::info("Screenshot captured: " + filename + " (" + 
                std::to_string(width) + "x" + std::to_string(height) + ")");
    return true;
}

double Renderer::getFrameTime() const {
    return m_frameTimeAccumulator;
}

double Renderer::getFPS() const {
    return m_fps;
}

// Private methods
bool Renderer::initializeGLFW() {
    glfwSetErrorCallback(glfwErrorCallback);
    
    if (!glfwInit()) {
        Logger::error("Failed to initialize GLFW");
        return false;
    }

    // Set OpenGL version to 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Additional window hints for better compatibility
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    return true;
}

bool Renderer::createWindow(int width, int height, const std::string& title, bool fullscreen) {
    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);
    
    if (!m_window) {
        Logger::error("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(m_window);
    
    // Set user pointer to this instance for callbacks
    glfwSetWindowUserPointer(m_window, this);
    
    // Set callbacks
    glfwSetKeyCallback(m_window, glfwKeyCallback);
    glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
    glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
    glfwSetScrollCallback(m_window, glfwScrollCallback);
    glfwSetFramebufferSizeCallback(m_window, glfwFramebufferSizeCallback);

    return true;
}

bool Renderer::initializeOpenGL() {
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        Logger::error("Failed to initialize GLEW: " + 
                     std::string(reinterpret_cast<const char*>(glewGetErrorString(err))));
        return false;
    }

    // Check OpenGL version
    if (!GLEW_VERSION_3_3) {
        Logger::error("OpenGL 3.3 not available");
        return false;
    }

    return true;
}

void Renderer::setupOpenGLState() {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable multisampling
    glEnable(GL_MULTISAMPLE);

    // Enable program point size for variable point sizes
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Set initial viewport
    int width, height;
    getFramebufferSize(width, height);
    glViewport(0, 0, width, height);
    
    // Set VSync
    setVSync(true);
}

GLuint Renderer::compileShader(GLenum type, const std::string& source) const {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        
        std::string shaderType;
        switch (type) {
            case GL_VERTEX_SHADER: shaderType = "vertex"; break;
            case GL_FRAGMENT_SHADER: shaderType = "fragment"; break;
            case GL_GEOMETRY_SHADER: shaderType = "geometry"; break;
            default: shaderType = "unknown"; break;
        }
        
        Logger::error("Shader compilation failed (" + shaderType + "): " + std::string(infoLog));
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

std::string Renderer::loadShaderFile(const std::string& filepath) const {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("Could not open shader file: " + filepath);
        return "";
    }

    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

void Renderer::updateFPS() {
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;
    
    m_frameTimeAccumulator = deltaTime * 1000.0; // Convert to milliseconds
    m_frameCount++;
    
    static double fpsTimer = 0.0;
    fpsTimer += deltaTime;
    
    if (fpsTimer >= 1.0) {
        m_fps = m_frameCount / fpsTimer;
        m_frameCount = 0;
        fpsTimer = 0.0;
    }
}

// Static callback implementations
void Renderer::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer && renderer->m_keyCallback) {
        renderer->m_keyCallback(key, scancode, action, mods);
    }
}

void Renderer::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer && renderer->m_mouseCallback) {
        renderer->m_mouseCallback(xpos, ypos);
    }
}

void Renderer::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer && renderer->m_mouseButtonCallback) {
        renderer->m_mouseButtonCallback(button, action, mods);
    }
}

void Renderer::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer && renderer->m_scrollCallback) {
        renderer->m_scrollCallback(xoffset, yoffset);
    }
}

void Renderer::glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    Renderer* renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (renderer && renderer->m_framebufferSizeCallback) {
        renderer->m_framebufferSizeCallback(width, height);
    }
}

void Renderer::glfwErrorCallback(int error, const char* description) {
    Logger::error("GLFW Error " + std::to_string(error) + ": " + std::string(description));
}