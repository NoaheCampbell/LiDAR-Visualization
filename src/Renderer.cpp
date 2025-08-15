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

#include <glm/gtc/matrix_transform.hpp>

static const char* kVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform float uPointSize;
uniform mat4 uProj;
uniform mat4 uView;
void main(){
  gl_Position = uProj * uView * vec4(aPos, 1.0);
  gl_PointSize = uPointSize;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main(){
  FragColor = vec4(uColor, 1.0);
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

  prog = createShaderProgram();
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_DEPTH_TEST);
  return true;
}

void Renderer::shutdown(){
  if(pointVbo) glDeleteBuffers(1, &pointVbo);
  if(pointVao) glDeleteVertexArrays(1, &pointVao);
  if(roverVbo) glDeleteBuffers(1, &roverVbo);
  if(roverVao) glDeleteVertexArrays(1, &roverVao);
  if(prog) glDeleteProgram(prog);
  pointVbo = pointVao = prog = 0;
}

void Renderer::resize(int width, int height){
  viewportWidth = width; viewportHeight = height;
}

void Renderer::updateRoverState(const std::string& roverId, const PosePacket& pose){
  auto& st = rovers[roverId];
  st.position = {pose.posX, pose.posY, pose.posZ};
  st.rotationDeg = {pose.rotXdeg, pose.rotYdeg, pose.rotZdeg};
}

void Renderer::setRoverColor(const std::string& roverId, const glm::vec3& color){
  rovers[roverId].color = color;
}

void Renderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj){
  viewM = view; projM = proj;
}

void Renderer::renderFrame(const std::vector<LidarPoint>& globalTerrain, float /*fps*/, int /*totalPoints*/){
  glViewport(0, 0, viewportWidth, viewportHeight);
  glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(prog);
  int locP = glGetUniformLocation(prog, "uProj");
  int locV = glGetUniformLocation(prog, "uView");
  int locS = glGetUniformLocation(prog, "uPointSize");
  int locC = glGetUniformLocation(prog, "uColor");
  glUniformMatrix4fv(locP, 1, GL_FALSE, &projM[0][0]);
  glUniformMatrix4fv(locV, 1, GL_FALSE, &viewM[0][0]);

  // Upload and draw points
  glBindVertexArray(pointVao);
  glBindBuffer(GL_ARRAY_BUFFER, pointVbo);
  if (!globalTerrain.empty()) {
    glBufferData(GL_ARRAY_BUFFER, globalTerrain.size() * sizeof(LidarPoint), globalTerrain.data(), GL_DYNAMIC_DRAW);
    glUniform1f(locS, 2.0f);
    glUniform3f(locC, 0.8f, 0.9f, 1.0f);
    glDrawArrays(GL_POINTS, 0, static_cast<int>(globalTerrain.size()));
  }
  glBindVertexArray(0);

  // Draw rover positions as larger points
  std::vector<glm::vec3> roverPts;
  roverPts.reserve(rovers.size());
  for (const auto& kv : rovers) roverPts.push_back(kv.second.position);
  if (!roverPts.empty()) {
    glBindVertexArray(roverVao);
    glBindBuffer(GL_ARRAY_BUFFER, roverVbo);
    glBufferData(GL_ARRAY_BUFFER, roverPts.size()*sizeof(glm::vec3), roverPts.data(), GL_DYNAMIC_DRAW);
    glUniform1f(locS, 8.0f);
    // Draw each rover with its color
    int offset = 0;
    for (const auto& kv : rovers) {
      const auto& st = kv.second;
      glUniform3f(locC, st.color.r, st.color.g, st.color.b);
      glDrawArrays(GL_POINTS, offset, 1);
      ++offset;
    }
    glBindVertexArray(0);
  }
}


