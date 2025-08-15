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
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "RoverProfiles.hpp"
#include "NetworkManager.hpp"
#include "DataAssembler.hpp"
#include "Renderer.hpp"

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

    NetworkManager net;
    DataAssembler assembler;
    assembler.setMaxPoints(2'000'000);

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

    auto last = std::chrono::high_resolution_clock::now();
    float fps = 0.0f;
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
        ++colorIdx;
    }
    float fovDeg = 60.0f;
    bool followSelected = true;
    bool freeFly = false;
    glm::vec3 followOffset = {30.0f, 30.0f, 20.0f};
    // Coordinate system up-axis (set to Y-up to match emulator data)
    const glm::vec3 worldUp = {0.0f, 1.0f, 0.0f};
    // Free-fly camera state
    float yawDeg = -45.0f, pitchDeg = -25.0f;
    float flySpeed = 20.0f;
    float mouseSensitivity = 0.15f; // deg per pixel
    bool invertYAxis = false;
    bool mouseLook = false;
    double lastMouseX = 0.0, lastMouseY = 0.0;

    std::string selectedRover = profiles.begin()->first;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        renderer.resize(w, h);

        // Timing
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        fps = (dt > 0.0f) ? (1.0f / dt) : fps;

        // Data maintenance
        assembler.maintenance(0.0);
        auto scans = assembler.retrieveCompleted();
        (void)scans;

        // UI frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Unified Control Panel
        ImGui::Begin("Control Panel");
        if (ImGui::CollapsingHeader("Rover Selector", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& [id, _] : profiles) {
                bool isSel = (id == selectedRover);
                if (ImGui::Selectable(id.c_str(), isSel)) selectedRover = id;
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
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Points: %zu", assembler.getGlobalTerrain().size());
            ImGui::Separator();
        }

        if (ImGui::CollapsingHeader("Mini-map", ImGuiTreeNodeFlags_DefaultOpen)) {
            const float mapSize = 220.0f;
            const float padFrac = 0.10f; // 10% padding around extents
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + mapSize, p0.y + mapSize), IM_COL32(20, 20, 20, 255));
            dl->AddRect(p0, ImVec2(p0.x + mapSize, p0.y + mapSize), IM_COL32(80, 80, 80, 255));

            // Compute dynamic bounds from rover positions
            float minX = FLT_MAX, maxX = -FLT_MAX, minYp = FLT_MAX, maxYp = -FLT_MAX;
            for (const auto& [id, st] : roverState) {
                float px = st.lastPose.posX;
                float py = (worldUp.y > 0.5f) ? st.lastPose.posZ : st.lastPose.posY;
                if (px < minX) minX = px; if (px > maxX) maxX = px;
                if (py < minYp) minYp = py; if (py > maxYp) maxYp = py;
            }
            if (minX > maxX) { minX = -100.0f; maxX = 100.0f; }
            if (minYp > maxYp) { minYp = -100.0f; maxYp = 100.0f; }
            float spanX = std::max(1.0f, maxX - minX);
            float spanY = std::max(1.0f, maxYp - minYp);
            // Add padding
            float padX = spanX * padFrac; float padY = spanY * padFrac;
            minX -= padX; maxX += padX; minYp -= padY; maxYp += padY;

            auto worldToMap = [&](float x, float y){
                float sx = (x - minX) / std::max(1e-3f, (maxX - minX));
                float sy = (y - minYp) / std::max(1e-3f, (maxYp - minYp));
                return ImVec2(p0.x + sx * mapSize, p0.y + (1.0f - sy) * mapSize);
            };

            // Helpers to draw looking cones/arrows
            auto drawCone = [&](float bx, float by, float headingRad, float lengthWorld, ImU32 colorFill, ImU32 colorLine){
                float halfFov = glm::radians(20.0f);
                ImVec2 b = worldToMap(bx, by);
                ImVec2 l = worldToMap(bx + cosf(headingRad - halfFov) * lengthWorld,
                                       by + sinf(headingRad - halfFov) * lengthWorld);
                ImVec2 r = worldToMap(bx + cosf(headingRad + halfFov) * lengthWorld,
                                       by + sinf(headingRad + halfFov) * lengthWorld);
                dl->AddTriangleFilled(b, l, r, colorFill);
                dl->AddLine(b, l, colorLine);
                dl->AddLine(b, r, colorLine);
            };

            float baseLen = 0.15f * std::max(maxX - minX, maxYp - minYp);
            if (baseLen < 5.0f) baseLen = 5.0f;

            // Draw rovers with facing cones
            for (const auto& [id, st] : roverState) {
                float px = st.lastPose.posX;
                float py = (worldUp.y > 0.5f) ? st.lastPose.posZ : st.lastPose.posY;
                ImVec2 pt = worldToMap(px, py);
                // Highlight selected rover
                ImU32 col = (id == selectedRover) ? IM_COL32(255, 210, 60, 255) : IM_COL32(60, 180, 255, 255);
                dl->AddCircleFilled(pt, 4.5f, col);
                dl->AddText(ImVec2(pt.x + 6, pt.y - 6), IM_COL32(200, 200, 200, 255), id.c_str());
                // Heading from rover yaw
                float yawDegRover = (worldUp.y > 0.5f) ? st.lastPose.rotYdeg : st.lastPose.rotZdeg;
                float yawRadRover = glm::radians(yawDegRover);
                drawCone(px, py, yawRadRover, baseLen, IM_COL32(60, 180, 255, 80), IM_COL32(60, 180, 255, 200));
            }

            // Draw camera position and viewing cone
            float cx = camPos.x;
            float cy = (worldUp.y > 0.5f) ? camPos.z : camPos.y;
            ImVec2 cpt = worldToMap(cx, cy);
            dl->AddCircleFilled(cpt, 5.0f, IM_COL32(80, 255, 120, 255));
            // Compute camera heading from target
            glm::vec2 dir = glm::normalize(glm::vec2(camTarget.x - camPos.x,
                                                     (worldUp.y > 0.5f) ? (camTarget.z - camPos.z) : (camTarget.y - camPos.y)));
            float camHeading = atan2f(dir.y, dir.x);
            drawCone(cx, cy, camHeading, baseLen * 0.9f, IM_COL32(80, 255, 120, 60), IM_COL32(80, 255, 120, 200));

            // Draw a simple crosshair/grid center
            ImVec2 c = worldToMap(0.0f, 0.0f);
            dl->AddLine(ImVec2(p0.x, c.y), ImVec2(p0.x + mapSize, c.y), IM_COL32(60,60,60,180));
            dl->AddLine(ImVec2(c.x, p0.y), ImVec2(c.x, p0.y + mapSize), IM_COL32(60,60,60,180));

            ImGui::Dummy(ImVec2(mapSize, mapSize));
            ImGui::Separator();
        }

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat3("Position", &camPos.x, -100.0f, 100.0f);
        ImGui::SliderFloat3("Target", &camTarget.x, -100.0f, 100.0f);
        ImGui::SliderFloat("FOV", &fovDeg, 20.0f, 120.0f);
        ImGui::Checkbox("Follow selected rover", &followSelected);
        ImGui::SameLine();
        ImGui::Checkbox("Free-fly (WASD + mouse)", &freeFly);
        ImGui::SliderFloat3("Follow offset", &followOffset.x, -200.0f, 200.0f);
        if (freeFly) {
            ImGui::SliderFloat("Fly speed", &flySpeed, 1.0f, 100.0f);
            ImGui::SliderFloat("Yaw", &yawDeg, -180.0f, 180.0f);
            ImGui::SliderFloat("Pitch", &pitchDeg, -89.0f, 89.0f);
            ImGui::SliderFloat("Mouse sensitivity", &mouseSensitivity, 0.01f, 1.0f);
            ImGui::Checkbox("Invert Y axis", &invertYAxis);
        }
        if (ImGui::Button("Center on selected (C)")) {
            const auto& p = roverState[selectedRover].lastPose;
            camTarget = {p.posX, p.posY, p.posZ};
            camPos = camTarget + followOffset;
        }
        }
        ImGui::End();

        // Render 3D
        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
        // Input for free-fly
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
        }
        // Follow mode updates
        if (followSelected && !freeFly) {
            const auto& p = roverState[selectedRover].lastPose;
            glm::vec3 tgt = {p.posX, p.posY, p.posZ};
            camTarget = tgt;
            camPos = tgt + followOffset;
        }
        // Keyboard center hotkey
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            const auto& p = roverState[selectedRover].lastPose;
            camTarget = {p.posX, p.posY, p.posZ};
            camPos = camTarget + followOffset;
        }
        glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 500.0f);
        glm::mat4 view = glm::lookAt(camPos, camTarget, worldUp);
        renderer.setViewProjection(view, proj);
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


