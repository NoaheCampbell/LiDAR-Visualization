#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

/**
 * OpenGL 3.3+ Renderer for Multi-Rover LiDAR Visualization
 * Handles window management, OpenGL context, and shader infrastructure
 */
class Renderer {
public:
    /**
     * Window event callbacks
     */
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseCallback = std::function<void(double xpos, double ypos)>;
    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    using ScrollCallback = std::function<void(double xoffset, double yoffset)>;
    using FramebufferSizeCallback = std::function<void(int width, int height)>;

    /**
     * Shader program wrapper
     */
    struct ShaderProgram {
        GLuint programId;
        std::unordered_map<std::string, GLint> uniformLocations;
        
        ShaderProgram() : programId(0) {}
        ~ShaderProgram();
        
        void use() const;
        void setUniform(const std::string& name, float value);
        void setUniform(const std::string& name, int value);
        void setUniform(const std::string& name, const glm::vec3& value);
        void setUniform(const std::string& name, const glm::vec4& value);
        void setUniform(const std::string& name, const glm::mat4& value);
        GLint getUniformLocation(const std::string& name);
    };

    Renderer();
    ~Renderer();

    /**
     * Initialize the renderer with window and OpenGL context
     * @param width Window width
     * @param height Window height
     * @param title Window title
     * @param fullscreen Create fullscreen window
     * @return true if initialization successful
     */
    bool initialize(int width = 1280, int height = 720, 
                   const std::string& title = "LiDAR Visualization", 
                   bool fullscreen = false);

    /**
     * Shutdown renderer and cleanup resources
     */
    void shutdown();

    /**
     * Check if the window should close
     * @return true if window should close
     */
    bool shouldClose() const;

    /**
     * Poll window events
     */
    void pollEvents();

    /**
     * Swap front and back buffers
     */
    void swapBuffers();

    /**
     * Begin frame rendering
     * @param clearColor Background clear color
     */
    void beginFrame(const glm::vec4& clearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));

    /**
     * End frame rendering
     */
    void endFrame();

    /**
     * Get window dimensions
     * @param width Output width
     * @param height Output height
     */
    void getWindowSize(int& width, int& height) const;

    /**
     * Get framebuffer dimensions
     * @param width Output width
     * @param height Output height
     */
    void getFramebufferSize(int& width, int& height) const;

    /**
     * Get aspect ratio
     * @return width / height
     */
    float getAspectRatio() const;

    /**
     * Set window size
     * @param width New width
     * @param height New height
     */
    void setWindowSize(int width, int height);

    /**
     * Toggle fullscreen mode
     * @return true if now fullscreen
     */
    bool toggleFullscreen();

    /**
     * Set window title
     * @param title New window title
     */
    void setWindowTitle(const std::string& title);

    /**
     * Enable/disable VSync
     * @param enabled VSync enabled
     */
    void setVSync(bool enabled);

    /**
     * Set window icon (32x32 RGBA)
     * @param pixels Icon pixel data
     * @param width Icon width
     * @param height Icon height
     */
    void setWindowIcon(const unsigned char* pixels, int width, int height);

    /**
     * Compile and link shader program
     * @param vertexSource Vertex shader source code
     * @param fragmentSource Fragment shader source code
     * @param geometrySource Optional geometry shader source (empty = none)
     * @return Shader program ID or 0 if failed
     */
    std::shared_ptr<ShaderProgram> createShaderProgram(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        const std::string& geometrySource = "");

    /**
     * Load shader program from files
     * @param vertexPath Vertex shader file path
     * @param fragmentPath Fragment shader file path
     * @param geometryPath Optional geometry shader file path
     * @return Shader program or nullptr if failed
     */
    std::shared_ptr<ShaderProgram> loadShaderProgram(
        const std::string& vertexPath,
        const std::string& fragmentPath,
        const std::string& geometryPath = "");

    /**
     * Get cached shader program by name
     * @param name Shader program name
     * @return Shader program or nullptr if not found
     */
    std::shared_ptr<ShaderProgram> getShaderProgram(const std::string& name) const;

    /**
     * Cache shader program with name
     * @param name Program name
     * @param program Shader program
     */
    void cacheShaderProgram(const std::string& name, std::shared_ptr<ShaderProgram> program);

    /**
     * Check for OpenGL errors and log them
     * @param operation Description of operation being checked
     * @return true if no errors
     */
    bool checkGLError(const std::string& operation = "") const;

    /**
     * Enable wireframe rendering
     * @param enabled Wireframe mode enabled
     */
    void setWireframeMode(bool enabled);

    /**
     * Enable depth testing
     * @param enabled Depth testing enabled
     */
    void setDepthTesting(bool enabled);

    /**
     * Enable face culling
     * @param enabled Face culling enabled
     */
    void setFaceCulling(bool enabled);

    /**
     * Set point size for point rendering
     * @param size Point size
     */
    void setPointSize(float size);

    /**
     * Enable point size attenuation
     * @param enabled Attenuation enabled
     */
    void setPointSizeAttenuation(bool enabled);

    /**
     * Get OpenGL version string
     * @return OpenGL version
     */
    std::string getOpenGLVersion() const;

    /**
     * Get renderer info
     * @return Renderer name and version
     */
    std::string getRendererInfo() const;

    /**
     * Get GLFW window handle
     * @return GLFW window pointer
     */
    GLFWwindow* getWindow() const { return m_window; }

    /**
     * Set callback functions
     */
    void setKeyCallback(const KeyCallback& callback);
    void setMouseCallback(const MouseCallback& callback);
    void setMouseButtonCallback(const MouseButtonCallback& callback);
    void setScrollCallback(const ScrollCallback& callback);
    void setFramebufferSizeCallback(const FramebufferSizeCallback& callback);

    /**
     * Capture screenshot to file
     * @param filename Output filename
     * @return true if successful
     */
    bool captureScreenshot(const std::string& filename) const;

    /**
     * Get current frame time in milliseconds
     * @return Frame time
     */
    double getFrameTime() const;

    /**
     * Get FPS counter
     * @return Current FPS
     */
    double getFPS() const;

private:
    // Window and context
    GLFWwindow* m_window;
    bool m_initialized;
    bool m_fullscreen;
    int m_windowWidth, m_windowHeight;
    int m_windowPosX, m_windowPosY;  // For fullscreen toggle
    std::string m_windowTitle;

    // Shader management
    std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_shaderCache;

    // Callbacks
    KeyCallback m_keyCallback;
    MouseCallback m_mouseCallback;
    MouseButtonCallback m_mouseButtonCallback;
    ScrollCallback m_scrollCallback;
    FramebufferSizeCallback m_framebufferSizeCallback;

    // Performance tracking
    double m_lastFrameTime;
    double m_frameTimeAccumulator;
    int m_frameCount;
    double m_fps;

    // Static callback wrappers
    static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void glfwErrorCallback(int error, const char* description);

    // Helper methods
    bool initializeGLFW();
    bool createWindow(int width, int height, const std::string& title, bool fullscreen);
    bool initializeOpenGL();
    void setupOpenGLState();
    GLuint compileShader(GLenum type, const std::string& source) const;
    std::string loadShaderFile(const std::string& filepath) const;
    void updateFPS();
};

#endif // RENDERER_H