#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <chrono>
#include <map>
#include <deque>
#include <string>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "RoverProfiles.hpp"
#include "NetworkManager.hpp"
#include "DataAssembler.hpp"
#include "Renderer.hpp"
#include "QuadtreeMap.hpp"

struct RoverState {
    PosePacket lastPose{};
    VehicleTelem lastTelem{};
    uint8_t localCmdBits = 0;
};

static void setupImGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

static void shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(1280, 720, "LiDAR Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Using system OpenGL headers; loader init not required on macOS core profile

    setupImGui(window);

    Renderer renderer;
    if (!renderer.init()) {
        shutdownImGui();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    auto profiles = getDefaultProfiles();
    std::map<std::string, RoverState> roverState;
    for (const auto& [id, prof] : profiles) { (void)prof; roverState[id] = RoverState{}; }
    // Smoothed positions for camera follow (per rover)
    std::unordered_map<std::string, glm::vec3> smoothedPos;

    NetworkManager net;
    DataAssembler assembler;
    ElevationMap elevMap;
    // Do not store raw points; elevation map is our primary product
    assembler.setStoreGlobalPoints(false);

    std::map<std::string, int> posePorts, lidarPorts, telemPorts, cmdPorts;
    for (const auto& [id, p] : profiles) {
        posePorts[id] = p.posePort;
        lidarPorts[id] = p.lidarPort;
        telemPorts[id] = p.telemPort;
        cmdPorts[id]  = p.cmdPort;
    }

    net.setPoseCallback([&](const std::string& id, const PosePacket& pose){
        roverState[id].lastPose = pose;
        renderer.updateRoverState(id, pose);
    });
    net.setLidarCallback([&](const std::string& id, const LidarPacketHeader& hdr, const LidarPoint* pts, size_t count){
        assembler.addChunk(id, hdr, pts, count);
    });
    net.setTelemCallback([&](const std::string& id, const VehicleTelem& t){
        roverState[id].lastTelem = t;
    });

    net.start(posePorts, lidarPorts, telemPorts);

    // Provide renderer a ground sampler backed by elevation map (z_mean). Use only when confident.
    renderer.setGroundSampler([&](float x, float z, float& outY, uint16_t& outN){
        return elevMap.getGroundAt(x, z, &outY, &outN);
    });

    auto last = std::chrono::high_resolution_clock::now();
    float fps = 0.0f;
    // Sliding-window FPS average
    std::deque<float> fpsWindow; // store recent frame durations (seconds)
    float fpsSum = 0.0f;         // sum of durations in window
    const float fpsWindowSeconds = 1.5f; // average over last ~1.5s
    // Simple free-fly camera params
    glm::vec3 camPos = {10.0f, 10.0f, 10.0f};
    glm::vec3 camTarget = {0.0f, 0.0f, 0.0f};
    // Assign distinct colors per rover
    std::vector<glm::vec3> palette = {
        {1.0f, 0.3f, 0.3f}, {0.3f, 1.0f, 0.3f}, {0.3f, 0.6f, 1.0f}, {1.0f, 0.8f, 0.2f}, {0.8f, 0.3f, 1.0f}
    };
    int colorIdx = 0;
    for (const auto& [id, _] : profiles) {
        renderer.setRoverColor(id, palette[colorIdx % palette.size()]);
        // Default forward offset of +1.0 m to account for sensor-vs-body origin
        renderer.setRoverModelOffset(id, glm::vec3(0.0f, 0.0f, 1.0f));
        ++colorIdx;
    }
    float fovDeg = 60.0f;
    bool followSelected = true;
    bool freeFly = false;
    glm::vec3 followOffset = {30.0f, 30.0f, 20.0f};
    // Orbit controls for follow camera
    float orbitYawDeg = -45.0f;   // rotation around Y
    float orbitPitchDeg = -25.0f; // tilt up/down
    float followRadius = 0.0f;
    // Coordinate system up-axis (set to Y-up to match emulator data)
    const glm::vec3 worldUp = {0.0f, 1.0f, 0.0f};
    // Free-fly camera state
    float yawDeg = -45.0f, pitchDeg = -25.0f;
    float flySpeed = 20.0f;
    float mouseSensitivity = 0.15f; // deg per pixel
    bool invertYAxis = false;
    bool mouseLook = false;
    double lastMouseX = 0.0, lastMouseY = 0.0;
    bool centerKeyDownPrev = false; // for edge-triggered 'C' center action
    bool pendingCenter = false;     // apply centering after camera logic
    bool suppressFollowOnce = false; // skip one follow update after centering

    std::string selectedRover = profiles.begin()->first;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        renderer.resize(w, h);

        // Timing
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        // Update FPS moving average window
        fpsWindow.push_back(dt);
        fpsSum += dt;
        while (fpsSum > fpsWindowSeconds && fpsWindow.size() > 1) {
            fpsSum -= fpsWindow.front();
            fpsWindow.pop_front();
        }
        float fpsAvg = (fpsSum > 1e-6f) ? (static_cast<float>(fpsWindow.size()) / fpsSum) : fps;
        fps = fpsAvg;

        // Data maintenance
        assembler.maintenance(0.0);
        auto scans = assembler.retrieveCompleted();
        // Integrate completed scans into elevation map
        for (const auto& sc : scans) {
            elevMap.integrateScan(sc.points, sc.timestamp);
        }
        // Upload dirty tiles to GPU on a budget (~10 MB per frame)
        renderer.ensureTerrainPipeline(elevMap.getGridNVertices());
        auto updates = elevMap.consumeDirtyTilesBudgeted(10 * 1024 * 1024);
        renderer.uploadDirtyTiles(updates);

        // UI frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Unified Control Panel
        ImGui::Begin("Control Panel");
        if (ImGui::CollapsingHeader("Rover Selector", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Horizontal buttons 1..N
            int idx = 0;
            for (const auto& [id, _] : profiles) {
                bool isSel = (id == selectedRover);
                if (isSel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f,0.45f,0.80f,1.0f));
                std::string label = id; // show ID as label
                if (ImGui::Button(label.c_str(), ImVec2(40, 0))) {
                    selectedRover = id;
                }
                if (isSel) ImGui::PopStyleColor();
                ++idx;
                if (idx < (int)profiles.size()) ImGui::SameLine();
            }
            // Number key shortcuts 1..5 (or up to number of rovers)
            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantCaptureKeyboard) {
                int keyIndex = -1;
                if (ImGui::IsKeyPressed(ImGuiKey_1, false)) keyIndex = 0;
                else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) keyIndex = 1;
                else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) keyIndex = 2;
                else if (ImGui::IsKeyPressed(ImGuiKey_4, false)) keyIndex = 3;
                else if (ImGui::IsKeyPressed(ImGuiKey_5, false)) keyIndex = 4;
                if (keyIndex >= 0) {
                    int i = 0; for (const auto& [id, _] : profiles) { if (i == keyIndex) { selectedRover = id; break; } ++i; }
                }
            }
            ImGui::Separator();
        }

        if (ImGui::CollapsingHeader("Telemetry & Commands", ImGuiTreeNodeFlags_DefaultOpen)) {
        const RoverState& rs = roverState[selectedRover];
        ImGui::Text("Pos: %.2f %.2f %.2f", rs.lastPose.posX, rs.lastPose.posY, rs.lastPose.posZ);
        ImGui::Text("Rot: %.1f %.1f %.1f", rs.lastPose.rotXdeg, rs.lastPose.rotYdeg, rs.lastPose.rotZdeg);

        uint8_t before = roverState[selectedRover].localCmdBits;
        for (int b = 0; b < 4; ++b) {
            bool bit = (before >> b) & 1u;
            std::string label = std::string("Button ") + std::to_string(b);
            if (ImGui::Checkbox(label.c_str(), &bit)) {
                if (bit) before |= (1u << b); else before &= ~(1u << b);
            }
        }
        if (before != roverState[selectedRover].localCmdBits) {
            roverState[selectedRover].localCmdBits = before;
            auto it = cmdPorts.find(selectedRover);
            if (it != cmdPorts.end()) {
                bool ok = net.sendCommand(selectedRover, before, it->second);
                if (!ok) {
                    // Simple retry once after ~50ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    net.sendCommand(selectedRover, before, it->second);
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Telemetry buttons: 0x%02X", rs.lastTelem.buttonStates);
        auto ts = net.getStreamTimestamps(selectedRover);
        ImGui::Text("Last Pose ts: %.3f", ts.lastPoseTs);
        ImGui::Text("Last Lidar ts: %.3f", ts.lastLidarTs);
        ImGui::Text("Last Telem ts: %.3f", ts.lastTelemTs);
        ImGui::Text("FPS (avg %.1fs): %.1f", fpsWindowSeconds, fps);
        ImGui::Text("Points: %zu", assembler.getGlobalTerrain().size());
        {
            float tdd = renderer.getTerrainDrawDistance();
            if (ImGui::SliderFloat("Terrain draw distance", &tdd, 200.0f, 3000.0f)) {
                renderer.setTerrainDrawDistance(tdd);
            }
            bool autoRange = renderer.getAutoHeightRange();
            if (ImGui::Checkbox("Auto height range", &autoRange)) {
                renderer.setAutoHeightRange(autoRange);
            }
            float minY = renderer.getObservedMinHeight();
            float maxY = renderer.getObservedMaxHeight();
            ImGui::Text("Observed range: [%.2f, %.2f] m", minY, maxY);
            float manMin, manMax; renderer.getManualHeightRange(manMin, manMax);
            if (!autoRange) {
                if (ImGui::DragFloat("Manual min Y", &manMin, 0.1f)) {
                    renderer.setManualHeightRange(manMin, manMax);
                }
                if (ImGui::DragFloat("Manual max Y", &manMax, 0.1f)) {
                    renderer.setManualHeightRange(manMin, manMax);
                }
            }
        }
            ImGui::Separator();
        }

        // Mini-map removed per request

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat3("Position", &camPos.x, -100.0f, 100.0f);
        ImGui::SliderFloat3("Target", &camTarget.x, -100.0f, 100.0f);
        ImGui::SliderFloat("FOV", &fovDeg, 20.0f, 120.0f);
        // Single toggle: when disabled, auto-follow is active
        ImGui::Checkbox("Free-fly (WASD + mouse)", &freeFly);
        followSelected = !freeFly;
        ImGui::SliderFloat3("Follow offset", &followOffset.x, -200.0f, 200.0f);
        if (freeFly) {
            ImGui::SliderFloat("Fly speed", &flySpeed, 1.0f, 100.0f);
            ImGui::SliderFloat("Yaw", &yawDeg, -180.0f, 180.0f);
            ImGui::SliderFloat("Pitch", &pitchDeg, -89.0f, 89.0f);
            ImGui::SliderFloat("Mouse sensitivity", &mouseSensitivity, 0.01f, 1.0f);
            ImGui::Checkbox("Invert Y axis", &invertYAxis);
        }
        if (ImGui::Button("Center on selected (C)")) {
            pendingCenter = true;
            suppressFollowOnce = true;
        }
        }
        ImGui::End();

        // Render 3D
        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
        // Keep orbit params in sync with current offset (in case user tweaks sliders)
        followRadius = sqrtf(followOffset.x*followOffset.x + followOffset.y*followOffset.y + followOffset.z*followOffset.z);
        if (followRadius < 1e-3f) followRadius = 1e-3f;
        {
            float yawFromOffset = atan2f(followOffset.z, followOffset.x);
            float pitchFromOffset = asinf(std::max(-1.0f, std::min(1.0f, followOffset.y / followRadius)));
            orbitYawDeg = yawFromOffset * 180.0f / 3.14159265f;
            orbitPitchDeg = pitchFromOffset * 180.0f / 3.14159265f;
        }
        // Derive follow state from freeFly
        followSelected = !freeFly;
        // Input for free-fly or follow-orbit rotation
        if (freeFly) {
            ImGuiIO& io = ImGui::GetIO();
            bool rotateBtn = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
                             glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            bool wantRotate = rotateBtn && !io.WantCaptureMouse;
            if (wantRotate) {
                double mx, my; glfwGetCursorPos(window, &mx, &my);
                if (!mouseLook) {
                    lastMouseX = mx; lastMouseY = my; mouseLook = true;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                double dx = mx - lastMouseX, dy = my - lastMouseY;
                lastMouseX = mx; lastMouseY = my;
                yawDeg   += static_cast<float>(dx) * mouseSensitivity;
                float signedDy = invertYAxis ? static_cast<float>(dy) : -static_cast<float>(dy);
                pitchDeg += signedDy * mouseSensitivity;
                if (pitchDeg > 89.0f) pitchDeg = 89.0f; if (pitchDeg < -89.0f) pitchDeg = -89.0f;
            } else if (mouseLook) {
                mouseLook = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            // Movement
            float yawRad = glm::radians(yawDeg);
            float pitchRad = glm::radians(pitchDeg);
            // Y-up forward vector
            glm::vec3 forward = glm::normalize(glm::vec3(
                cosf(pitchRad) * cosf(yawRad),
                sinf(pitchRad),
                cosf(pitchRad) * sinf(yawRad)));
            glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));
            float move = flySpeed * dt;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += forward * move;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= forward * move;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= right * move;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += right * move;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camPos -= up * move;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camPos += up * move;
            camTarget = camPos + forward * 10.0f;
            followSelected = false;
        } else if (followSelected && !suppressFollowOnce) {
            // Allow orbit camera rotation while following the rover
            ImGuiIO& io = ImGui::GetIO();
            bool rotateBtn = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
                             glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            bool wantRotate = rotateBtn && !io.WantCaptureMouse;
            if (wantRotate) {
                double mx, my; glfwGetCursorPos(window, &mx, &my);
                if (!mouseLook) {
                    lastMouseX = mx; lastMouseY = my; mouseLook = true;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                double dx = mx - lastMouseX, dy = my - lastMouseY;
                lastMouseX = mx; lastMouseY = my;
                orbitYawDeg   += static_cast<float>(dx) * mouseSensitivity;
                float signedDy = invertYAxis ? static_cast<float>(dy) : -static_cast<float>(dy);
                orbitPitchDeg += signedDy * mouseSensitivity;
                if (orbitPitchDeg > 89.0f) orbitPitchDeg = 89.0f; if (orbitPitchDeg < -89.0f) orbitPitchDeg = -89.0f;
            } else if (mouseLook) {
                mouseLook = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            // Scroll wheel zoom (adjust follow distance)
            if (!io.WantCaptureMouse && fabsf(io.MouseWheel) > 1e-4f) {
                const float zoomStep = 1.15f; // per wheel notch
                followRadius /= powf(zoomStep, io.MouseWheel); // wheel>0 => zoom in (smaller radius)
                if (followRadius < 2.0f) followRadius = 2.0f;
                if (followRadius > 1000.0f) followRadius = 1000.0f;
            }
            // Recompute offset from orbit yaw/pitch
            float yawRad = orbitYawDeg * 3.14159265f / 180.0f;
            float pitchRad = orbitPitchDeg * 3.14159265f / 180.0f;
            followOffset.x = followRadius * cosf(pitchRad) * cosf(yawRad);
            followOffset.y = followRadius * sinf(pitchRad);
            followOffset.z = followRadius * cosf(pitchRad) * sinf(yawRad);
        }
        // Follow mode updates
        if (followSelected && !freeFly && !suppressFollowOnce) {
            const auto& p = roverState[selectedRover].lastPose;
            // Update smoothed position for the selected rover (EMA with ~0.3s time constant)
            auto &sp = smoothedPos[selectedRover];
            if (sp == glm::vec3(0.0f)) {
                sp = glm::vec3(p.posX, p.posY, p.posZ);
            } else {
                float tau = 0.3f; // seconds
                float alpha = 1.0f - expf(-dt / std::max(1e-3f, tau));
                glm::vec3 meas(p.posX, p.posY, p.posZ);
                sp = sp + alpha * (meas - sp);
            }
            // Camera target follows smoothed rover pose directly (no ground coupling)
            static glm::vec3 camTargetSmoothed = sp;
            float tauT = 0.4f; float alphaT = 1.0f - expf(-dt / std::max(1e-3f, tauT));
            camTargetSmoothed = camTargetSmoothed + alphaT * (sp - camTargetSmoothed);
            camTarget = camTargetSmoothed;
            camPos = camTarget + followOffset;
        }
        // Keyboard center hotkey (edge-triggered) -> set pending flag
        {
            bool cDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            if (cDown && !centerKeyDownPrev) { pendingCenter = true; suppressFollowOnce = true; }
            centerKeyDownPrev = cDown;
        }

        // Apply pending center AFTER follow/free-fly camera updates to avoid flicker
        if (pendingCenter) {
            const auto& p = roverState[selectedRover].lastPose;
            auto itSp = smoothedPos.find(selectedRover);
            glm::vec3 base = (itSp != smoothedPos.end() && itSp->second != glm::vec3(0.0f))
                             ? itSp->second
                             : glm::vec3(p.posX, p.posY, p.posZ);
            float gy = base.y; uint16_t gn = 0;
            if (elevMap.getGroundAt(base.x, base.z, &gy, &gn)) {
                camTarget = {base.x, gy + 0.8f, base.z};
            } else {
                camTarget = base;
            }
            camPos = camTarget + followOffset;
            // If in free-fly, align yaw/pitch so forward looks at the rover
            if (freeFly) {
                glm::vec3 dir = glm::normalize(camTarget - camPos);
                float newYawDeg = atan2f(dir.z, dir.x) * 180.0f / 3.14159265f;
                float newPitchDeg = asinf(std::max(-1.0f, std::min(1.0f, dir.y))) * 180.0f / 3.14159265f;
                yawDeg = newYawDeg;
                pitchDeg = newPitchDeg;
                if (pitchDeg > 89.0f) pitchDeg = 89.0f; if (pitchDeg < -89.0f) pitchDeg = -89.0f;
            }
            pendingCenter = false;
            suppressFollowOnce = false;
        }
        glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 500.0f);
        glm::mat4 view = glm::lookAt(camPos, camTarget, worldUp);
        renderer.setViewProjection(view, proj);
        // Ensure rover rendering ignores terrain orientation for stability
        renderer.setAlignToTerrain(false);
        renderer.renderFrame(assembler.getGlobalTerrain(), fps, (int)assembler.getGlobalTerrain().size());

        // ImGui draw
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    net.stop();
    renderer.shutdown();
    shutdownImGui();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


