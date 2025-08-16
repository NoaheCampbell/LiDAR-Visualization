#include "Renderer.hpp"

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "QuadtreeMap.hpp"

static const char* kVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform float uPointSize;
uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vWorldPos;
void main(){
  vec4 worldPos = uModel * vec4(aPos, 1.0);
  vWorldPos = worldPos.xyz;
  // Use inverse-transpose for correct normal when non-uniform scaling is applied
  mat3 N = transpose(inverse(mat3(uModel)));
  vNormal = normalize(N * aNormal);
  gl_Position = uProj * uView * worldPos;
  gl_PointSize = uPointSize;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
in vec3 vNormal;
in vec3 vWorldPos;
uniform vec3 uLightDir; // world-space directional light (normalized)
uniform bool uUseLighting;
uniform bool uColorByHeight;
uniform float uMinY;
uniform float uMaxY;
uniform vec3 uLowColor;
uniform vec3 uHighColor;
void main(){
  if(!uUseLighting){
    vec3 base = uColor;
    if (uColorByHeight) {
      float denom = max(uMaxY - uMinY, 1e-5);
      float t = clamp((vWorldPos.y - uMinY) / denom, 0.0, 1.0);
      base = mix(uLowColor, uHighColor, t);
    }
    FragColor = vec4(base, 1.0);
    return;
  }
  vec3 n = normalize(vNormal);
  float ndl = max(dot(n, -normalize(uLightDir)), 0.0);
  float ambient = 0.25;
  float diffuse = 0.75 * ndl;
  vec3 lit = uColor * (ambient + diffuse);
  FragColor = vec4(lit, 1.0);
}
)GLSL";

// Simple terrain shader (position only + flat color with basic lambert)
static const char* kTerrainVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uProj;
uniform mat4 uView;
out vec3 vNormal;
out float vHeight;
void main(){
  vNormal = aNormal;
  vHeight = aPos.y;
  gl_Position = uProj * uView * vec4(aPos, 1.0);
}
)GLSL";
static const char* kTerrainFS = R"GLSL(
#version 330 core
out vec4 FragColor;
in vec3 vNormal;
in float vHeight;
uniform vec3 uLightDir;
uniform bool uColorByHeight;
uniform float uMinY;
uniform float uMaxY;
uniform vec3 uLowColor;
uniform vec3 uHighColor;
void main(){
  vec3 n = normalize(vNormal);
  float ndl = max(dot(n, -normalize(uLightDir)), 0.0);
  float a = 0.55;              // brighter ambient for visibility
  float d = 0.50 * ndl;        // slightly softer diffuse
  vec3 base;
  if (uColorByHeight) {
    float denom = max(uMaxY - uMinY, 1e-5);
    float t = clamp((vHeight - uMinY) / denom, 0.0, 1.0);
    base = mix(uLowColor, uHighColor, t);
  } else {
    base = vec3(0.42,0.55,0.42); // lighter green
  }
  FragColor = vec4(base*(a+d), 1.0);
}
)GLSL";

static unsigned int compile(unsigned int type, const char* src){
  unsigned int s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if(!ok){
    char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
    std::cerr << "Shader compile error: " << log << std::endl;
  }
  return s;
}

unsigned int Renderer::createShaderProgram(){
  unsigned int vs = compile(GL_VERTEX_SHADER, kVS);
  unsigned int fs = compile(GL_FRAGMENT_SHADER, kFS);
  unsigned int p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

bool Renderer::init(){
  glGenVertexArrays(1, &pointVao);
  glGenBuffers(1, &pointVbo);
  glBindVertexArray(pointVao);
  glBindBuffer(GL_ARRAY_BUFFER, pointVbo);
  glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);
  glBindVertexArray(0);

  glGenVertexArrays(1, &roverVao);
  glGenBuffers(1, &roverVbo);
  glBindVertexArray(roverVao);
  glBindBuffer(GL_ARRAY_BUFFER, roverVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*5, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);
  glBindVertexArray(0);

  // Rover orientation lines (position + heading endpoint)
  glGenVertexArrays(1, &roverLineVao);
  glGenBuffers(1, &roverLineVbo);
  glBindVertexArray(roverLineVao);
  glBindBuffer(GL_ARRAY_BUFFER, roverLineVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*10, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);
  glBindVertexArray(0);

  prog = createShaderProgram();
  // Terrain program
  {
    unsigned int vs = compile(GL_VERTEX_SHADER, kTerrainVS);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, kTerrainFS);
    terrainProg = glCreateProgram();
    glAttachShader(terrainProg, vs);
    glAttachShader(terrainProg, fs);
    glLinkProgram(terrainProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
  }
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);

  // Create a unit cube mesh centered at origin with normals
  // 24 unique vertices (4 per face) to get sharp edges with per-face normals
  struct VN { float px, py, pz, nx, ny, nz; };
  const VN cubeVerts[] = {
    // +X face
    {+0.5f,-0.5f,-0.5f,  1,0,0}, {+0.5f,+0.5f,-0.5f,  1,0,0}, {+0.5f,+0.5f,+0.5f,  1,0,0}, {+0.5f,-0.5f,+0.5f,  1,0,0},
    // -X face
    {-0.5f,-0.5f,+0.5f, -1,0,0}, {-0.5f,+0.5f,+0.5f, -1,0,0}, {-0.5f,+0.5f,-0.5f, -1,0,0}, {-0.5f,-0.5f,-0.5f, -1,0,0},
    // +Y face
    {-0.5f,+0.5f,-0.5f,  0,1,0}, {-0.5f,+0.5f,+0.5f,  0,1,0}, {+0.5f,+0.5f,+0.5f,  0,1,0}, {+0.5f,+0.5f,-0.5f,  0,1,0},
    // -Y face
    {-0.5f,-0.5f,+0.5f,  0,-1,0}, {-0.5f,-0.5f,-0.5f,  0,-1,0}, {+0.5f,-0.5f,-0.5f,  0,-1,0}, {+0.5f,-0.5f,+0.5f,  0,-1,0},
    // +Z face
    {-0.5f,-0.5f,+0.5f,  0,0,1}, {+0.5f,-0.5f,+0.5f,  0,0,1}, {+0.5f,+0.5f,+0.5f,  0,0,1}, {-0.5f,+0.5f,+0.5f,  0,0,1},
    // -Z face
    {+0.5f,-0.5f,-0.5f,  0,0,-1}, {-0.5f,-0.5f,-0.5f, 0,0,-1}, {-0.5f,+0.5f,-0.5f, 0,0,-1}, {+0.5f,+0.5f,-0.5f, 0,0,-1},
  };
  const unsigned short cubeIdx[] = {
    0,1,2, 0,2,3,
    4,5,6, 4,6,7,
    8,9,10, 8,10,11,
    12,13,14, 12,14,15,
    16,17,18, 16,18,19,
    20,21,22, 20,22,23
  };
  roverMeshIndexCount = (int)(sizeof(cubeIdx)/sizeof(cubeIdx[0]));
  glGenVertexArrays(1, &roverMeshVao);
  glGenBuffers(1, &roverMeshVbo);
  glGenBuffers(1, &roverMeshEbo);
  glBindVertexArray(roverMeshVao);
  glBindBuffer(GL_ARRAY_BUFFER, roverMeshVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, roverMeshEbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VN), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VN), (void*)(sizeof(float)*3));
  glBindVertexArray(0);
  return true;
}

void Renderer::shutdown(){
  if(pointVbo) glDeleteBuffers(1, &pointVbo);
  if(pointVao) glDeleteVertexArrays(1, &pointVao);
  if(roverVbo) glDeleteBuffers(1, &roverVbo);
  if(roverVao) glDeleteVertexArrays(1, &roverVao);
  if(roverLineVbo) glDeleteBuffers(1, &roverLineVbo);
  if(roverLineVao) glDeleteVertexArrays(1, &roverLineVao);
  if(roverMeshEbo) glDeleteBuffers(1, &roverMeshEbo);
  if(roverMeshVbo) glDeleteBuffers(1, &roverMeshVbo);
  if(roverMeshVao) glDeleteVertexArrays(1, &roverMeshVao);
  if(prog) glDeleteProgram(prog);
  if(terrainProg) glDeleteProgram(terrainProg);
  if(sharedEbo) glDeleteBuffers(1, &sharedEbo);
  for (auto &kv : gpuTiles) {
    if (kv.second.vbo) glDeleteBuffers(1, &kv.second.vbo);
    if (kv.second.ebo && kv.second.ebo != sharedEbo) glDeleteBuffers(1, &kv.second.ebo);
    if (kv.second.vao) glDeleteVertexArrays(1, &kv.second.vao);
  }
  gpuTiles.clear();
  pointVbo = pointVao = prog = 0;
}

void Renderer::resize(int width, int height){
  viewportWidth = width; viewportHeight = height;
}

void Renderer::updateRoverState(const std::string& roverId, const PosePacket& pose){
  auto& st = rovers[roverId];
  glm::vec3 newPos = {pose.posX, pose.posY, pose.posZ};
  glm::vec3 newRot = {pose.rotXdeg, pose.rotYdeg, pose.rotZdeg};
  // Alpha-Beta (g-h) filter for position and yaw (prediction + correction)
  double ts = pose.timestamp;
  double dt = (st.kfInitialized && st.lastPoseTs > 0.0) ? std::max(1e-3, ts - st.lastPoseTs) : 0.1;
  st.lastPoseTs = ts;
  const float g = 0.25f;  // position gain
  const float h = 0.6f;   // velocity gain
  const float gYaw = 0.25f, hYaw = 0.6f;
  if (!st.kfInitialized) {
    st.kfPos = newPos;
    st.kfVel = glm::vec3(0.0f);
    st.kfYawDeg = newRot.y;
    st.kfYawRateDeg = 0.0f;
    st.kfInitialized = true;
  } else {
    // Predict
    st.kfPos += st.kfVel * static_cast<float>(dt);
    st.kfYawDeg += st.kfYawRateDeg * static_cast<float>(dt);
    // Normalize yaw to [-180,180]
    auto normYaw = [](float yaw){ while (yaw > 180.f) yaw -= 360.f; while (yaw < -180.f) yaw += 360.f; return yaw; };
    st.kfYawDeg = normYaw(st.kfYawDeg);
    float measYaw = normYaw(newRot.y);
    // Innovation
    glm::vec3 r = newPos - st.kfPos;
    float ryaw = normYaw(measYaw - st.kfYawDeg);
    // Correct
    st.kfPos += g * r;
    st.kfVel += (h / static_cast<float>(dt)) * r;
    st.kfYawDeg = normYaw(st.kfYawDeg + gYaw * ryaw);
    st.kfYawRateDeg += (hYaw / static_cast<float>(dt)) * ryaw;
  }
  st.smoothedPosition = st.kfPos;
  st.smoothedRotationDeg = {newRot.x, st.kfYawDeg, newRot.z};
  // Update noise score as EMA of instantaneous displacement magnitude
  float inst = glm::length(newPos - st.position);
  const float alphaNoise = 0.1f;
  st.noiseScore = (1.0f - alphaNoise) * st.noiseScore + alphaNoise * inst;
  st.position = newPos;
  st.rotationDeg = newRot;
}

void Renderer::setRoverColor(const std::string& roverId, const glm::vec3& color){
  rovers[roverId].color = color;
}

void Renderer::setRoverModelOffset(const std::string& roverId, const glm::vec3& localOffsetRUF){
  rovers[roverId].modelOffsetLocal = localOffsetRUF;
}

void Renderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj){
  viewM = view; projM = proj;
}

void Renderer::renderFrame(const std::vector<LidarPoint>& globalTerrain, float fps, int /*totalPoints*/){
  glViewport(0, 0, viewportWidth, viewportHeight);
  glClearColor(0.03f, 0.035f, 0.04f, 1.0f); // deeper background for contrast
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Draw terrain if any
  drawTerrain();

  glUseProgram(prog);
  int locP = glGetUniformLocation(prog, "uProj");
  int locV = glGetUniformLocation(prog, "uView");
  int locS = glGetUniformLocation(prog, "uPointSize");
  int locC = glGetUniformLocation(prog, "uColor");
  int locM = glGetUniformLocation(prog, "uModel");
  int locL = glGetUniformLocation(prog, "uLightDir");
  int locUse = glGetUniformLocation(prog, "uUseLighting");
  int locCBH_P = glGetUniformLocation(prog, "uColorByHeight");
  int locMinY_P = glGetUniformLocation(prog, "uMinY");
  int locMaxY_P = glGetUniformLocation(prog, "uMaxY");
  int locLow_P = glGetUniformLocation(prog, "uLowColor");
  int locHigh_P = glGetUniformLocation(prog, "uHighColor");
  glUniformMatrix4fv(locP, 1, GL_FALSE, &projM[0][0]);
  glUniformMatrix4fv(locV, 1, GL_FALSE, &viewM[0][0]);
  // Fixed light from above-left
  glUniform3f(locL, 0.3f, 1.0f, 0.6f);
  // Default model to identity for non-mesh draws
  glm::mat4 identityModel(1.0f);
  glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(identityModel));

  // Upload and draw points (disabled by default when terrain is rendered to reduce visual clutter)
  if (renderPoints) {
    glBindVertexArray(pointVao);
    glBindBuffer(GL_ARRAY_BUFFER, pointVbo);
    if (!globalTerrain.empty()) {
      glBufferData(GL_ARRAY_BUFFER, globalTerrain.size() * sizeof(LidarPoint), globalTerrain.data(), GL_DYNAMIC_DRAW);
      glUniform1f(locS, 2.0f);
      glUniform3f(locC, 0.8f, 0.85f, 0.9f);
      glUniform1i(locUse, 0);
      // Configure height-based coloring for points
      glUniform1i(locCBH_P, 1);
      // Prefer visible range if available for better contrast
      float minYp = autoHeightRange ? (useVisibleHeightRange ? lastVisibleMinY : observedMinY) : manualMinY;
      float maxYp = autoHeightRange ? (useVisibleHeightRange ? lastVisibleMaxY : observedMaxY) : manualMaxY;
      if (!std::isfinite(minYp) || !std::isfinite(maxYp)) { minYp = 0.0f; maxYp = 1.0f; }
      if (maxYp - minYp < 1e-4f) { maxYp = minYp + 1.0f; }
      glUniform1f(locMinY_P, minYp);
      glUniform1f(locMaxY_P, maxYp);
      glUniform3f(locLow_P, lowColor.r, lowColor.g, lowColor.b);
      glUniform3f(locHigh_P, highColor.r, highColor.g, highColor.b);
      glDrawArrays(GL_POINTS, 0, static_cast<int>(globalTerrain.size()));
    }
    glBindVertexArray(0);
  }

  // Helper to estimate local ground Y from nearby LiDAR points
  auto estimateGroundY = [&](const glm::vec3& posXZ) -> float {
    // Prefer elevation map via groundSampler if available
    if (groundSampler) {
      float y = posXZ.y; uint16_t n = 0;
      bool ok = groundSampler(posXZ.x, posXZ.z, y, n);
      if (ok) return y;
    }
    if (globalTerrain.empty()) return posXZ.y;
    const float radius1 = 3.0f;
    const float radius2 = 6.0f;
    const float r1sq = radius1 * radius1;
    const float r2sq = radius2 * radius2;
    std::vector<float> heights;
    heights.reserve(64);
    auto sample = [&](float r2limit){
      const int stride = 32; // downsample for speed
      for (size_t i = 0; i < globalTerrain.size(); i += stride) {
        const auto& p = globalTerrain[i];
        float dx = p.x - posXZ.x;
        float dz = p.z - posXZ.z;
        float d2 = dx*dx + dz*dz;
        if (d2 <= r2limit) {
          heights.push_back(p.y);
          if (heights.size() >= 64) break;
        }
      }
    };
    sample(r1sq);
    if (heights.size() < 8) { heights.clear(); sample(r2sq); }
    if (heights.empty()) return posXZ.y;
    std::nth_element(heights.begin(), heights.begin() + heights.size()/4, heights.end());
    float ground = heights[heights.size()/4]; // lower quartile to avoid high outliers
    return ground;
  };

  // Accumulate heading arrows to render after drawing rovers
  std::vector<glm::vec3> lines;
  lines.reserve(rovers.size() * 6);
  float dtSec = (fps > 1e-3f) ? (1.0f / fps) : 0.016f;
  // Draw each rover as a larger cube placed slightly above the ground, oriented to terrain normal, with a red nose
  if (!rovers.empty()) {
    glBindVertexArray(roverMeshVao);
    for (const auto& kv : rovers) {
      const auto& st = kv.second;
      // Bigger body
      const glm::vec3 baseScale = glm::vec3(3.2f, 1.4f, 2.4f);
      float yawRad = glm::radians(st.smoothedRotationDeg.y);

      // Always follow ground with symmetric rate limiting; no physics and no popping
      auto& mut = rovers[kv.first];
      glm::vec3 center = mut.smoothedPosition;
      // Prefer confident elevation map sample directly for stability; fallback to local estimate
      float groundY = mut.smoothedPosition.y;
      uint16_t nconf = 0;
      bool okSample = false;
      if (groundSampler) {
        float ytmp = groundY; uint16_t ntmp = 0;
        okSample = groundSampler(mut.smoothedPosition.x, mut.smoothedPosition.z, ytmp, ntmp);
        if (okSample) { groundY = ytmp; nconf = ntmp; }
      }
      if (!okSample) {
        groundY = estimateGroundY(mut.smoothedPosition);
      }
      // Robust baseline: deadband + time-constant EMA to suppress noise
      if (!mut.initialized) {
        mut.groundYFiltered = groundY;
      } else {
        float diff = groundY - mut.groundYFiltered;
        const float deadband = 0.15f; // ignore small changes entirely
        if (fabsf(diff) > deadband) {
          const float tau = 1.2f; // seconds
          float alpha = 1.0f - expf(-dtSec / std::max(0.01f, tau));
          // scale update by how far outside the band we are
          float excess = diff > 0 ? (diff - deadband) : (diff + deadband);
          mut.groundYFiltered += alpha * excess;
        }
      }
      float desiredY = mut.groundYFiltered + 0.5f * baseScale.y + 0.35f;
      if (!mut.initialized) {
        // Start from current smoothed Y to avoid an initial upward jump
        mut.lastRenderCenterY = center.y;
        mut.initialized = true;
      }
      float maxRise = 0.05f; // m/frame up (tighter)
      float maxFall = 0.10f; // m/frame down (tighter)
      float deltaY = desiredY - mut.lastRenderCenterY;
      if (deltaY > maxRise) center.y = mut.lastRenderCenterY + maxRise;
      else if (deltaY < -maxFall) center.y = mut.lastRenderCenterY - maxFall;
      else center.y = desiredY;
      mut.lastRenderCenterY = center.y;

      // Terrain alignment (optional). When disabled, keep upright
      glm::vec3 up(0.0f, 1.0f, 0.0f);
      glm::vec3 n = up;
      if (alignToTerrain) {
        bool haveNormal = false;
        if (groundSampler) {
          float step = 0.75f; // meters
          float yL, yR, yD, yU; uint16_t tmp;
          bool okL = groundSampler(center.x - step, center.z, yL, tmp);
          bool okR = groundSampler(center.x + step, center.z, yR, tmp);
          bool okD = groundSampler(center.x, center.z - step, yD, tmp);
          bool okU = groundSampler(center.x, center.z + step, yU, tmp);
          if (okL && okR && okD && okU) {
            float dYdX = (yR - yL) / (2.0f * step);
            float dYdZ = (yU - yD) / (2.0f * step);
            n = glm::normalize(glm::vec3(-dYdX, 1.0f, -dYdZ));
            haveNormal = true;
          }
        }
        if (!haveNormal) n = up;
      }

      // Build orientation that preserves yaw around the terrain normal
      glm::vec3 fwd = glm::normalize(glm::vec3(sinf(yawRad), 0.0f, cosf(yawRad)));
      glm::vec3 fwdT = alignToTerrain ? glm::normalize(fwd - glm::dot(fwd, n) * n) : fwd;
      if (!std::isfinite(fwdT.x)) fwdT = glm::normalize(glm::cross(n, glm::vec3(1,0,0)));
      if (glm::length(fwdT) < 1e-3f) fwdT = glm::normalize(glm::cross(n, glm::vec3(0,0,1)));
      glm::vec3 right = glm::normalize(glm::cross(fwdT, n));

      // Body
      glm::mat4 model(1.0f);
      model = glm::translate(model, center);
      // orientation basis (columns)
      glm::mat4 basis(1.0f);
      basis[0] = glm::vec4(right, 0.0f);
      basis[1] = glm::vec4(n, 0.0f);
      basis[2] = glm::vec4(fwdT, 0.0f);
      // apply local model offset (Right, Up, Forward) in aligned basis
      glm::vec3 off = rovers.at(kv.first).modelOffsetLocal;
      glm::vec3 worldOff = right * off.x + n * off.y + fwdT * off.z;
      model = glm::translate(model, worldOff) * basis;
      model = glm::scale(model, baseScale);
      glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(model));
      glUniform3f(locC, st.color.r, st.color.g, st.color.b);
      glUniform1i(locUse, 1);
      glUniform1i(locCBH_P, 0);
      glDrawElements(GL_TRIANGLES, roverMeshIndexCount, GL_UNSIGNED_SHORT, 0);

      // Nose marker on roof to indicate heading (use +Z forward when yaw=0)
      glm::vec3 roof = center + worldOff + n * (0.5f * baseScale.y);
      glm::vec3 noseWorld = roof + fwdT * (0.55f * baseScale.z);
      glm::mat4 nose(1.0f);
      nose = glm::translate(nose, noseWorld);
      glm::mat4 nbasis(1.0f);
      nbasis[0] = glm::vec4(right, 0.0f);
      nbasis[1] = glm::vec4(n, 0.0f);
      nbasis[2] = glm::vec4(fwdT, 0.0f);
      nose = nose * nbasis;
      nose = glm::scale(nose, glm::vec3(0.3f, 0.2f, 0.5f));
      glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(nose));
      glUniform3f(locC, 1.0f, 0.25f, 0.25f);
      glUniform1i(locUse, 1);
      glUniform1i(locCBH_P, 0);
      glDrawElements(GL_TRIANGLES, roverMeshIndexCount, GL_UNSIGNED_SHORT, 0);

      // Build heading arrow locked to the model transform
      glm::vec3 base = center + worldOff + n * (0.6f * baseScale.y);
      float len = 3.8f;
      glm::vec3 tip = base + fwdT * len;
      float headAng = glm::radians(22.0f);
      // Arrowhead rotated around the local up-axis (n), using model-aligned right vector
      glm::vec3 headDirL = glm::normalize(fwdT * cosf(headAng) - right * sinf(headAng));
      glm::vec3 headDirR = glm::normalize(fwdT * cosf(headAng) + right * sinf(headAng));
      float headLen = 1.0f;
      glm::vec3 leftPt = tip - headDirL * headLen;
      glm::vec3 rightPt = tip - headDirR * headLen;
      // Shaft
      lines.push_back(base);
      lines.push_back(tip);
      // Head
      lines.push_back(tip);
      lines.push_back(leftPt);
      lines.push_back(tip);
      lines.push_back(rightPt);
    }
    glBindVertexArray(0);
  }

  // Draw heading arrows, using positions computed during rover rendering
  if (!lines.empty()) {
    glBindVertexArray(roverLineVao);
    glBindBuffer(GL_ARRAY_BUFFER, roverLineVbo);
    glBufferData(GL_ARRAY_BUFFER, lines.size()*sizeof(glm::vec3), lines.data(), GL_DYNAMIC_DRAW);
    glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(identityModel));
    glUniform1f(locS, 1.0f);
    glUniform1i(locUse, 0);
    // Force lines to ignore height-based color (keep white)
    glUniform1i(locCBH_P, 0);
    int idx = 0;
    for (const auto& kv : rovers) {
      (void)kv;
      glUniform3f(locC, 1.0f, 1.0f, 1.0f);
      glDrawArrays(GL_LINES, idx*6 + 0, 2);
      glDrawArrays(GL_LINES, idx*6 + 2, 2);
      glDrawArrays(GL_LINES, idx*6 + 4, 2);
      ++idx;
    }
    glBindVertexArray(0);
  }
}

void Renderer::ensureTerrainPipeline(int gridNVertices){
  if (terrainGridN == gridNVertices && sharedEbo != 0) return;
  // Rebuild shared index buffer for a grid made of (gridN-1)x(gridN-1) quads
  terrainGridN = gridNVertices;
  if (sharedEbo) { glDeleteBuffers(1, &sharedEbo); sharedEbo = 0; }
  std::vector<unsigned int> indices;
  int N = gridNVertices;
  indices.reserve((N-1)*(N-1)*6);
  for (int z = 0; z < N-1; ++z) {
    for (int x = 0; x < N-1; ++x) {
      unsigned int i0 = z * N + x;
      unsigned int i1 = z * N + (x+1);
      unsigned int i2 = (z+1) * N + x;
      unsigned int i3 = (z+1) * N + (x+1);
      // two triangles
      indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
      indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
    }
  }
  glGenBuffers(1, &sharedEbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedEbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}

void Renderer::uploadDirtyTiles(const std::vector<TileUpdate>& updates){
  if (updates.empty()) return;
  // Assume ensureTerrainPipeline was called by caller
  int N = terrainGridN;
  struct VN { float px, py, pz, nx, ny, nz; };
  for (const auto& up : updates) {
    long long key = (static_cast<long long>(up.key.tx) << 32) ^ (static_cast<unsigned long long>(up.key.tz) & 0xffffffffull);
    TileGpu &gpu = gpuTiles[key];
    if (gpu.vao == 0) {
      glGenVertexArrays(1, &gpu.vao);
      glGenBuffers(1, &gpu.vbo);
      gpu.ebo = sharedEbo;
      glBindVertexArray(gpu.vao);
      glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(VN) * N * N, nullptr, GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VN), (void*)0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VN), (void*)(sizeof(float)*3));
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedEbo);
      gpu.indexCount = (N-1)*(N-1)*6;
      gpu.tileSize = up.tileSize;
    }
    // Build vertex buffer from heights. Compute normals via central differences.
    std::vector<VN> verts; verts.resize(N*N);
    float localMinY = std::numeric_limits<float>::infinity();
    float localMaxY = -std::numeric_limits<float>::infinity();
    // We need tile size to compute XZ; approximate using spacing from heights grid: assume contiguous spacing of 1.0 in index.
    // The caller should set model transform if needed; here we bake XZ assuming tile indices map to meters 0..tileSize.
    // We'll infer spacing from count by assuming the original tile size is encoded elsewhere; not available here.
    // For now, use 1.0 spacing; the world positioning will be done by absolute XZ in heights builder, so stash XZ there instead.
    // QuadtreeMap::buildHeightGrid writes absolute heights but not XZ; reconstruct XZ from tx/tz and tile size using N.
    // We cannot access tile size here; rely on adjacency count via positions computed outside. We'll adjust in drawTerrain using uniforms? Simpler: encode absolute XZ in heights vector is not possible.
    // Instead, we assume positions are absolute X,Z embedded via linear mapping: x = (tx*tileSize) + (i/(N-1))*tileSize. We'll store tx,tz in gpu and compute in CPU now requiring tileSize. We don't have it. We'll approximate tileSize=32.
    const float tileSize = up.tileSize;
    float originX = (float)up.key.tx * tileSize;
    float originZ = (float)up.key.tz * tileSize;
    float step = tileSize / float(N-1);
    auto h = [&](int j, int i)->float { return up.heights[j*N + i]; };
    for (int j = 0; j < N; ++j) {
      for (int i = 0; i < N; ++i) {
        float x = originX + i * step;
        float z = originZ + j * step;
        float y = h(j,i);
        if (y < localMinY) localMinY = y;
        if (y > localMaxY) localMaxY = y;
        // Snap shared edges (right/top) to global ground sampler to avoid cracks
        if ((i == N - 1 || j == N - 1) && groundSampler) {
          float ySnap = y; uint16_t nTmp = 0;
          if (groundSampler(x, z, ySnap, nTmp)) {
            y = ySnap;
          }
        }
        // central differences for normal
        int im = std::max(i-1, 0), ip = std::min(i+1, N-1);
        int jm = std::max(j-1, 0), jp = std::min(j+1, N-1);
        float hx0 = h(j, im), hx1 = h(j, ip);
        float hz0 = h(jm, i), hz1 = h(jp, i);
        glm::vec3 dx(step, hx1 - hx0, 0.0f);
        glm::vec3 dz(0.0f, hz1 - hz0, step);
        glm::vec3 n = glm::normalize(glm::cross(dz, dx));
        VN v{ x, y, z, n.x, n.y, n.z };
        verts[j*N + i] = v;
      }
    }
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(VN), verts.data(), GL_DYNAMIC_DRAW);
    gpu.tx = up.key.tx; gpu.tz = up.key.tz;
    // Update global observed range
    if (std::isfinite(localMinY)) observedMinY = std::min(observedMinY, localMinY);
    if (std::isfinite(localMaxY)) observedMaxY = std::max(observedMaxY, localMaxY);
    gpu.minY = localMinY;
    gpu.maxY = localMaxY;
  }
}

void Renderer::drawTerrain(){
  if (gpuTiles.empty() || terrainProg == 0) return;
  glUseProgram(terrainProg);
  int locP = glGetUniformLocation(terrainProg, "uProj");
  int locV = glGetUniformLocation(terrainProg, "uView");
  int locL = glGetUniformLocation(terrainProg, "uLightDir");
  int locCBH = glGetUniformLocation(terrainProg, "uColorByHeight");
  int locMinY = glGetUniformLocation(terrainProg, "uMinY");
  int locMaxY = glGetUniformLocation(terrainProg, "uMaxY");
  int locLow = glGetUniformLocation(terrainProg, "uLowColor");
  int locHigh = glGetUniformLocation(terrainProg, "uHighColor");
  glUniformMatrix4fv(locP, 1, GL_FALSE, &projM[0][0]);
  glUniformMatrix4fv(locV, 1, GL_FALSE, &viewM[0][0]);
  glUniform3f(locL, 0.3f, 1.0f, 0.6f);
  glUniform1i(locCBH, 1);
  // Optionally compute range from visible tiles near camera for stronger contrast
  if (autoHeightRange && useVisibleHeightRange) {
    lastVisibleMinY = std::numeric_limits<float>::infinity();
    lastVisibleMaxY = -std::numeric_limits<float>::infinity();
    glm::mat4 invV = glm::inverse(viewM);
    glm::vec3 camPos(invV[3].x, invV[3].y, invV[3].z);
    for (auto &kv : gpuTiles) {
      const TileGpu &gpu = kv.second;
      float tileCenterX = (gpu.tx + 0.5f) * gpu.tileSize;
      float tileCenterZ = (gpu.tz + 0.5f) * gpu.tileSize;
      float dx = tileCenterX - camPos.x;
      float dz = tileCenterZ - camPos.z;
      float dist2 = dx*dx + dz*dz;
      if (dist2 > terrainDrawDistance * terrainDrawDistance) continue;
      if (std::isfinite(gpu.minY)) lastVisibleMinY = std::min(lastVisibleMinY, gpu.minY);
      if (std::isfinite(gpu.maxY)) lastVisibleMaxY = std::max(lastVisibleMaxY, gpu.maxY);
    }
  }
  float minY = autoHeightRange ? (useVisibleHeightRange ? lastVisibleMinY : observedMinY) : manualMinY;
  float maxY = autoHeightRange ? (useVisibleHeightRange ? lastVisibleMaxY : observedMaxY) : manualMaxY;
  if (!std::isfinite(minY) || !std::isfinite(maxY)) { minY = 0.0f; maxY = 1.0f; }
  if (maxY - minY < 1e-4f) { maxY = minY + 1.0f; }
  glUniform1f(locMinY, minY);
  glUniform1f(locMaxY, maxY);
  // Blue (low) -> red (high)
  glUniform3f(locLow, lowColor.r, lowColor.g, lowColor.b);
  glUniform3f(locHigh, highColor.r, highColor.g, highColor.b);
  for (auto &kv : gpuTiles) {
    const TileGpu &gpu = kv.second;
    // Simple distance culling to maintain FPS
    // Estimate camera position from inverse of view matrix (extract translation)
    glm::mat4 invV = glm::inverse(viewM);
    glm::vec3 camPos(invV[3].x, invV[3].y, invV[3].z);
    float tileCenterX = (gpu.tx + 0.5f) * gpu.tileSize;
    float tileCenterZ = (gpu.tz + 0.5f) * gpu.tileSize;
    float dx = tileCenterX - camPos.x;
    float dz = tileCenterZ - camPos.z;
    float dist2 = dx*dx + dz*dz;
    if (dist2 > terrainDrawDistance * terrainDrawDistance) continue;
    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedEbo);
    glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, 0);
  }
  glBindVertexArray(0);
}


