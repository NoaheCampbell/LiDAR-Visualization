#include "Renderer.h"
#include "Camera.h"
#include "RoverRenderer.h"
#include "PointCloudRenderer.h"
#include "ButtonUtils.h"
#include "NetworkManager.h"
#include "DataAssembler.h"
#include "Logger.h"
#include "Constants.h"

#include "imgui_stub.h"
#define ImGui_ImplGlfw_InitForOpenGL(x,y) ImGui_ImplGlfw::InitForOpenGL(x,y)
#define ImGui_ImplGlfw_Shutdown() ImGui_ImplGlfw::Shutdown()
#define ImGui_ImplGlfw_NewFrame() ImGui_ImplGlfw::NewFrame()
#define ImGui_ImplOpenGL3_Init(x) ImGui_ImplOpenGL3::Init(x)
#define ImGui_ImplOpenGL3_Shutdown() ImGui_ImplOpenGL3::Shutdown()
#define ImGui_ImplOpenGL3_NewFrame() ImGui_ImplOpenGL3::NewFrame()
#define ImGui_ImplOpenGL3_RenderDrawData(x) ImGui_ImplOpenGL3::RenderDrawData(x)
// GetDrawData handled in stub
#include "gl_compat.h"
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>

/**
 * Multi-Rover LiDAR Visualization Application
 * Complete integration of all rendering and networking components
 */
class LidarVisualizationApp {
public:
    LidarVisualizationApp() 
        : m_running(false)
        , m_selectedRover(1)
        , m_showDiagnostics(true)
        , m_showTelemetry(true)
        , m_showControls(true)
        , m_lastFrameTime(0.0)
        , m_logger(Logger::getInstance()) {
    }

    ~LidarVisualizationApp() {
        shutdown();
    }

    bool initialize() {
        m_logger.info("Initializing Multi-Rover LiDAR Visualization Application");

        // Initialize renderer first
        Renderer::Config rendererConfig;
        rendererConfig.windowTitle = "Multi-Rover LiDAR Visualization";
        rendererConfig.enableVSync = false; // Disable for performance
        rendererConfig.enableMSAA = true;
        
        if (!m_renderer.initialize(rendererConfig)) {
            m_logger.error("Failed to initialize renderer");
            return false;
        }

        // Initialize camera
        m_camera.initialize(glm::vec3(0.0f, 20.0f, 50.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        m_camera.setMode(Camera::Mode::FREE_FLY);

        // Initialize rover renderer
        if (!m_roverRenderer.initialize()) {
            m_logger.error("Failed to initialize rover renderer");
            return false;
        }

        // Initialize point cloud renderer
        if (!m_pointCloudRenderer.initialize()) {
            m_logger.error("Failed to initialize point cloud renderer");
            return false;
        }

        // Initialize button utils
        if (!m_buttonUtils.initialize()) {
            m_logger.error("Failed to initialize button utils");
            return false;
        }

        // Initialize data assembler
        if (!m_dataAssembler.initialize()) {
            m_logger.error("Failed to initialize data assembler");
            return false;
        }

        // Initialize network manager
        if (!m_networkManager.initialize()) {
            m_logger.error("Failed to initialize network manager");
            return false;
        }

        // Configure all rovers for network communication
        for (uint8_t i = 1; i <= 5; ++i) {
            const auto* profile = RoverProfiles::getRover(i);
            if (!profile) {
                m_logger.error("Failed to get rover profile for rover " + std::to_string(i));
                return false;
            }
            
            if (!m_networkManager.configureRover(i, 
                profile->ports.posePort,
                profile->ports.lidarPort, 
                profile->ports.telemetryPort,
                profile->ports.commandPort)) {
                m_logger.error("Failed to configure rover " + std::to_string(i));
                return false;
            }
            m_logger.info("Configured rover " + std::to_string(i) + " with ports: pose=" + 
                         std::to_string(profile->ports.posePort) + ", lidar=" + 
                         std::to_string(profile->ports.lidarPort) + ", telemetry=" + 
                         std::to_string(profile->ports.telemetryPort) + ", command=" + 
                         std::to_string(profile->ports.commandPort));
        }

        // Load shaders
        if (!loadShaders()) {
            m_logger.error("Failed to load shaders");
            return false;
        }

        // Setup ImGui
        if (!initializeImGui()) {
            m_logger.error("Failed to initialize ImGui");
            return false;
        }

        // Setup callbacks
        setupCallbacks();

        m_running = true;
        m_logger.info("Application initialized successfully");
        return true;
    }

    void run() {
        if (!m_running) {
            m_logger.error("Application not initialized");
            return;
        }

        m_logger.info("Starting main application loop");
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        
        while (m_running && !m_renderer.shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Update subsystems
            update(deltaTime);

            // Render frame
            render();

            // Maintain target frame rate
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        m_logger.info("Application loop ended");
    }

    void shutdown() {
        if (!m_running) {
            return;
        }

        m_logger.info("Shutting down application");

        // Shutdown ImGui
        if (m_imguiInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        // Shutdown subsystems in reverse order
        m_networkManager.shutdown();
        m_dataAssembler.shutdown();
        m_buttonUtils.shutdown();
        m_pointCloudRenderer.shutdown();
        m_roverRenderer.shutdown();
        m_renderer.shutdown();

        m_running = false;
        m_logger.info("Application shutdown complete");
    }

private:
    bool loadShaders() {
        m_logger.info("Loading shaders");

        // Load basic shader for rovers
        if (!m_renderer.loadShader("basic", "shaders/basic.vert", "shaders/basic.frag")) {
            m_logger.error("Failed to load basic shader");
            return false;
        }

        // Load point cloud shader
        if (!m_renderer.loadShader("pointcloud", "shaders/pointcloud.vert", "shaders/pointcloud.frag")) {
            m_logger.error("Failed to load point cloud shader");
            return false;
        }

        return true;
    }

    bool initializeImGui() {
        m_logger.info("Initializing ImGui");

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        if (!ImGui_ImplGlfw_InitForOpenGL(m_renderer.getWindow(), true)) {
            m_logger.error("Failed to initialize ImGui GLFW backend");
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            m_logger.error("Failed to initialize ImGui OpenGL3 backend");
            return false;
        }

        m_imguiInitialized = true;
        return true;
    }

    void setupCallbacks() {
        // Set up renderer callbacks
        m_renderer.setKeyCallback([this](int key, int scancode, int action, int mods) {
            handleKeyInput(key, scancode, action, mods);
        });

        m_renderer.setCursorCallback([this](double xpos, double ypos) {
            handleMouseInput(xpos, ypos);
        });

        m_renderer.setScrollCallback([this](double xoffset, double yoffset) {
            handleScrollInput(xoffset, yoffset);
        });

        // Set up data assembler callback
        m_dataAssembler.setScanCompleteCallback([this](const DataAssembler::CompleteLidarScan& scan) {
            m_pointCloudRenderer.addPointCloudScan(scan.roverId, scan);
        });

        // Set up network manager callbacks
        m_networkManager.setPoseCallback([this](uint8_t roverId, const PosePacket& pose) {
            m_roverRenderer.updateRoverPose(roverId, pose);
            m_camera.updateRoverPosition(roverId, pose);
            m_pointCloudRenderer.updateRoverPosition(roverId, glm::vec3(pose.posX, pose.posY, pose.posZ));
        });

        m_networkManager.setLidarCallback([this](uint8_t roverId, const LidarPacket& packet) {
            m_dataAssembler.processLidarPacket(roverId, packet);
        });

        m_networkManager.setTelemetryCallback([this](uint8_t roverId, const VehicleTelem& telem) {
            m_buttonUtils.updateButtonStates(telem.buttonStates);
        });

        // Set up button utils callback
        m_buttonUtils.setCommandCallback([this](uint8_t roverId, const CommandPacket& packet) -> bool {
            return m_networkManager.sendCommand(roverId, packet);
        });
    }

    void update(float deltaTime) {
        m_lastFrameTime = deltaTime * 1000.0f; // Convert to ms

        // Update camera
        m_camera.update(deltaTime);

        // Poll for network data
        // Network manager runs automatically in background threads

        // Update input state
        updateInput();

        // Update rover connections
        updateRoverConnections();
    }

    void updateInput() {
        // Handle camera movement
        Camera::MovementState movement{};
        
        if (m_renderer.isKeyPressed(GLFW_KEY_W)) movement.moveForward = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_S)) movement.moveBackward = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_A)) movement.moveLeft = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_D)) movement.moveRight = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_Q)) movement.moveUp = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_E)) movement.moveDown = true;
        if (m_renderer.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) movement.fastMode = true;

        m_camera.setMovementState(movement);

        // Handle rover selection
        for (int i = 1; i <= 5; ++i) {
            if (m_renderer.isKeyPressed(GLFW_KEY_1 + i - 1)) {
                m_selectedRover = i;
                break;
            }
        }

        // Handle camera mode switching
        if (m_renderer.isKeyPressed(GLFW_KEY_F)) {
            m_camera.setMode(Camera::Mode::FREE_FLY);
        }
        if (m_renderer.isKeyPressed(GLFW_KEY_T)) {
            m_camera.setMode(Camera::Mode::FOLLOW);
            m_camera.setFollowTarget(m_selectedRover);
        }
    }

    void updateRoverConnections() {
        // Update rover connection status based on recent pose updates
        for (int i = 1; i <= 5; ++i) {
            auto roverState = m_roverRenderer.getRoverState(i);
            double timeSinceUpdate = getCurrentTime() - roverState.lastUpdateTime;
            bool connected = timeSinceUpdate < (Constants::Performance::ROVER_DISCONNECT_TIMEOUT_MS / 1000.0);
            
            m_roverRenderer.setRoverConnected(i, connected);
        }
    }

    void render() {
        m_renderer.beginFrame();
        m_renderer.clear(glm::vec4(Constants::Colors::BACKGROUND_COLOR[0], 
                                  Constants::Colors::BACKGROUND_COLOR[1],
                                  Constants::Colors::BACKGROUND_COLOR[2],
                                  Constants::Colors::BACKGROUND_COLOR[3]));

        // Get camera matrices
        float aspectRatio = m_renderer.getAspectRatio();
        glm::mat4 viewMatrix = m_camera.getViewMatrix();
        glm::mat4 projectionMatrix = m_camera.getProjectionMatrix(aspectRatio);
        glm::vec3 viewPos = m_camera.getPosition();

        // Render point clouds first (with blending)
        m_renderer.useShader("pointcloud");
        setupPointCloudUniforms(viewMatrix, projectionMatrix, viewPos);
        m_pointCloudRenderer.render(viewMatrix, projectionMatrix, viewPos);

        // Render rovers
        m_renderer.useShader("basic");
        setupBasicUniforms(viewMatrix, projectionMatrix, viewPos);
        m_roverRenderer.render(viewMatrix, projectionMatrix, viewPos);

        // Render ImGui
        renderImGui();

        m_renderer.endFrame();
    }

    void setupBasicUniforms(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& viewPos) {
        m_renderer.setUniform("uViewMatrix", viewMatrix);
        m_renderer.setUniform("uProjectionMatrix", projectionMatrix);
        m_renderer.setUniform("uViewPos", viewPos);
        
        // Lighting parameters
        m_renderer.setUniform("uLightDirection", glm::vec3(-0.3f, -1.0f, -0.3f));
        m_renderer.setUniform("uLightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        m_renderer.setUniform("uAmbientColor", glm::vec3(0.3f, 0.3f, 0.3f));
        m_renderer.setUniform("uAmbientStrength", 0.1f);
        m_renderer.setUniform("uShininess", 32.0f);
        m_renderer.setUniform("uSpecularStrength", 0.5f);
        m_renderer.setUniform("uEnableLighting", true);
        m_renderer.setUniform("uUseVertexColor", false);
    }

    void setupPointCloudUniforms(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& viewPos) {
        glm::mat4 modelMatrix = glm::mat4(1.0f); // Identity for world-space point clouds
        
        m_renderer.setUniform("uModelMatrix", modelMatrix);
        m_renderer.setUniform("uViewMatrix", viewMatrix);
        m_renderer.setUniform("uProjectionMatrix", projectionMatrix);
        m_renderer.setUniform("uViewPos", viewPos);
        m_renderer.setUniform("uPointSize", m_pointCloudRenderer.getConfig().pointSize);
        m_renderer.setUniform("uUseIntensity", m_pointCloudRenderer.getConfig().useIntensity);
        m_renderer.setUniform("uUseDistance", m_pointCloudRenderer.getConfig().useDistance);
        m_renderer.setUniform("uMaxDistance", m_pointCloudRenderer.getConfig().maxRenderDistance);
        m_renderer.setUniform("uMinDistance", m_pointCloudRenderer.getConfig().minRenderDistance);
        m_renderer.setUniform("uAlpha", m_pointCloudRenderer.getConfig().alpha);
        m_renderer.setUniform("uCircularPoints", m_pointCloudRenderer.getConfig().circularPoints);
    }

    void renderImGui() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (m_showTelemetry) {
            renderTelemetryPanel();
        }

        if (m_showControls) {
            renderControlPanel();
        }

        if (m_showDiagnostics) {
            renderDiagnosticsPanel();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui_GetDrawData());
    }

    void renderTelemetryPanel() {
        ImGui::Begin("Rover Telemetry", &m_showTelemetry);

        // Rover selector
        ImGui::Text("Selected Rover: %d", m_selectedRover);
        ImGui::SameLine();
        if (ImGui::Button("Follow")) {
            m_camera.setMode(Camera::Mode::FOLLOW);
            m_camera.setFollowTarget(m_selectedRover);
        }

        ImGui::Separator();

        // Show telemetry for selected rover
        auto roverState = m_roverRenderer.getRoverState(m_selectedRover);
        
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                   roverState.position.x, roverState.position.y, roverState.position.z);
        ImGui::Text("Rotation: (%.1f°, %.1f°, %.1f°)", 
                   roverState.rotation.x, roverState.rotation.y, roverState.rotation.z);
        ImGui::Text("Connected: %s", roverState.isConnected ? "Yes" : "No");
        ImGui::Text("Visible: %s", roverState.isVisible ? "Yes" : "No");
        
        if (roverState.lastUpdateTime > 0.0) {
            double timeSinceUpdate = getCurrentTime() - roverState.lastUpdateTime;
            ImGui::Text("Last Update: %.1fs ago", timeSinceUpdate);
        } else {
            ImGui::Text("Last Update: Never");
        }

        ImGui::Separator();

        // Button states
        ImGui::Text("Button States:");
        for (int i = 0; i < 4; ++i) {
            bool pressed = m_buttonUtils.isButtonPressed(i);
            ImGui::SameLine();
            if (ImGui::Button((std::string("B") + std::to_string(i)).c_str())) {
                // Toggle button when clicked
                m_buttonUtils.updateSingleButton(i, !pressed);
                if (roverState.isConnected) {
                    m_buttonUtils.sendButtonCommand(m_selectedRover, m_buttonUtils.getButtonMask());
                }
            }
            if (pressed) {
                ImGui::SameLine();
                ImGui::TextColored(ImGui::ImVec4(0, 1, 0, 1), "ON");
            }
        }

        ImGui::End();
    }

    void renderControlPanel() {
        ImGui::Begin("Controls", &m_showControls);

        // Camera controls
        ImGui::Text("Camera Mode:");
        if (ImGui::RadioButton("Free Fly", m_camera.getMode() == Camera::Mode::FREE_FLY)) {
            m_camera.setMode(Camera::Mode::FREE_FLY);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Follow", m_camera.getMode() == Camera::Mode::FOLLOW)) {
            m_camera.setMode(Camera::Mode::FOLLOW);
            m_camera.setFollowTarget(m_selectedRover);
        }

        ImGui::Separator();

        // Rover selection
        ImGui::Text("Select Rover:");
        for (int i = 1; i <= 5; ++i) {
            if (ImGui::RadioButton((std::string("Rover ") + std::to_string(i)).c_str(), m_selectedRover == i)) {
                m_selectedRover = i;
                if (m_camera.getMode() == Camera::Mode::FOLLOW) {
                    m_camera.setFollowTarget(m_selectedRover);
                }
            }
            if (i < 5) ImGui::SameLine();
        }

        ImGui::Separator();

        // Visibility controls
        ImGui::Text("Rover Visibility:");
        for (int i = 1; i <= 5; ++i) {
            bool visible = m_roverRenderer.isRoverVisible(i);
            if (ImGui::Checkbox((std::string("R") + std::to_string(i)).c_str(), &visible)) {
                m_roverRenderer.setRoverVisible(i, visible);
            }
            if (i < 5) ImGui::SameLine();
        }

        ImGui::Text("Point Cloud Visibility:");
        for (int i = 1; i <= 5; ++i) {
            bool visible = m_pointCloudRenderer.isRoverVisible(i);
            if (ImGui::Checkbox((std::string("PC") + std::to_string(i)).c_str(), &visible)) {
                m_pointCloudRenderer.setRoverVisible(i, visible);
            }
            if (i < 5) ImGui::SameLine();
        }

        ImGui::Separator();

        // Button Command Controls
        ImGui::Text("Button Commands (Rover %d):", m_selectedRover);
        auto roverState = m_roverRenderer.getRoverState(m_selectedRover);
        
        // Display connection status
        if (roverState.isConnected) {
            ImGui::TextColored(ImGui::ImVec4(0, 1, 0, 1), "Connected");
        } else {
            ImGui::TextColored(ImGui::ImVec4(1, 0, 0, 1), "Disconnected");
        }
        
        // Button checkboxes for command sending
        for (int buttonIdx = 0; buttonIdx < 4; ++buttonIdx) {
            bool newState = m_buttonUtils.isButtonPressed(buttonIdx);
            bool oldState = newState;
            
            if (ImGui::Checkbox((std::string("Button ") + std::to_string(buttonIdx)).c_str(), &newState)) {
                // Button state changed - send command
                if (roverState.isConnected) {
                    m_buttonUtils.updateSingleButton(buttonIdx, newState);
                    uint8_t commandByte = m_buttonUtils.getButtonMask();
                    
                    // Create command packet and send via NetworkManager
                    CommandPacket commandPacket = m_buttonUtils.createButtonCommand(m_selectedRover, commandByte);
                    bool success = m_networkManager.sendCommand(m_selectedRover, commandPacket);
                    
                    if (success) {
                        m_logger.info("Button command sent to rover " + std::to_string(m_selectedRover) + 
                                     " - Button " + std::to_string(buttonIdx) + " set to " + 
                                     (newState ? "ON" : "OFF"));
                    } else {
                        m_logger.error("Failed to send button command to rover " + std::to_string(m_selectedRover));
                    }
                } else {
                    m_logger.warn("Cannot send button command - rover " + std::to_string(m_selectedRover) + " is not connected");
                    // Revert the checkbox state since command couldn't be sent
                    newState = oldState;
                }
            }
            
            if (buttonIdx < 3) ImGui::SameLine();
        }

        ImGui::Separator();

        // Point cloud controls
        auto pcConfig = m_pointCloudRenderer.getConfig();
        if (ImGui::SliderFloat("Point Size", &pcConfig.pointSize, 1.0f, 10.0f)) {
            m_pointCloudRenderer.setConfig(pcConfig);
        }
        if (ImGui::SliderFloat("Alpha", &pcConfig.alpha, 0.1f, 1.0f)) {
            m_pointCloudRenderer.setConfig(pcConfig);
        }

        ImGui::End();
    }

    void renderDiagnosticsPanel() {
        ImGui::Begin("Diagnostics", &m_showDiagnostics);

        // Performance metrics
        auto renderStats = m_renderer.getStats();
        ImGui::Text("FPS: %.1f", renderStats.fps);
        ImGui::Text("Frame Time: %.2f ms", m_lastFrameTime);
        ImGui::Text("Draw Calls: %llu", renderStats.drawCalls);

        ImGui::Separator();

        // Network statistics
        // Get network stats for first rover as an example
        auto networkStats = m_networkManager.getStats(1, NetworkManager::DataType::TELEMETRY);
        ImGui::Text("Packets Received: %llu", networkStats.packetsReceived);
        ImGui::Text("Bytes Received: %llu", networkStats.bytesReceived);
        ImGui::Text("Packets Lost: %llu", networkStats.packetsLost);

        ImGui::Separator();

        // Point cloud statistics
        auto pcStats = m_pointCloudRenderer.getStats();
        ImGui::Text("Total Points: %llu", pcStats.totalPointsAvailable);
        ImGui::Text("Rendered Points: %llu", pcStats.totalPointsRendered);
        ImGui::Text("GPU Memory: %.1f MB", pcStats.gpuMemoryUsed / (1024.0 * 1024.0));

        ImGui::Separator();

        // Data assembler statistics
        auto assemblerStats = m_dataAssembler.getStats();
        ImGui::Text("Scans Completed: %llu", assemblerStats.scansCompleted);
        ImGui::Text("Scans Timed Out: %llu", assemblerStats.scansTimedOut);
        ImGui::Text("Avg Assembly Time: %.2f ms", assemblerStats.averageAssemblyTime);

        ImGui::End();
    }

    void handleKeyInput(int key, int scancode, int action, int mods) {
        // Toggle UI panels
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_F1:
                    m_showTelemetry = !m_showTelemetry;
                    break;
                case GLFW_KEY_F2:
                    m_showControls = !m_showControls;
                    break;
                case GLFW_KEY_F3:
                    m_showDiagnostics = !m_showDiagnostics;
                    break;
                case GLFW_KEY_ESCAPE:
                    m_running = false;
                    break;
            }
        }
    }

    void handleMouseInput(double xpos, double ypos) {
        static double lastX = xpos;
        static double lastY = ypos;
        static bool firstMouse = true;

        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float deltaX = static_cast<float>(xpos - lastX);
        float deltaY = static_cast<float>(ypos - lastY);
        lastX = xpos;
        lastY = ypos;

        // Only process mouse movement if right mouse button is pressed
        if (m_renderer.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            m_camera.processMouseMovement(deltaX, deltaY);
        }
    }

    void handleScrollInput(double xoffset, double yoffset) {
        m_camera.processMouseScroll(static_cast<float>(yoffset));
    }

    double getCurrentTime() const {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }

private:
    // Core subsystems
    Renderer m_renderer;
    Camera m_camera;
    RoverRenderer m_roverRenderer;
    PointCloudRenderer m_pointCloudRenderer;
    ButtonUtils m_buttonUtils;
    NetworkManager m_networkManager;
    DataAssembler m_dataAssembler;

    // Application state
    std::atomic<bool> m_running;
    uint8_t m_selectedRover;
    bool m_imguiInitialized = false;

    // UI state
    bool m_showDiagnostics;
    bool m_showTelemetry;
    bool m_showControls;

    // Performance tracking
    float m_lastFrameTime;

    // Logger
    Logger::SystemLogger& m_logger;
};

int main() {
    try {
        LidarVisualizationApp app;
        
        if (!app.initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        app.run();
        app.shutdown();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown application error" << std::endl;
        return -1;
    }
}