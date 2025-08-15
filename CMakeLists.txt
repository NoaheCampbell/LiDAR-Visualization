cmake_minimum_required(VERSION 3.20)
project(lidar_challenge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_VIEWER "Build the LiDAR viewer GUI" ON)
option(BUILD_EMULATOR "Build the rover emulator" ON)

include(FetchContent)

if(BUILD_VIEWER)
  # GLFW
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)

  # glm (header-only)
  FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
  )
  set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glm)

  # Dear ImGui
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
  )
  FetchContent_MakeAvailable(imgui)

  # Create an imgui library with the GLFW + OpenGL3 backends
  add_library(imgui_glfw_opengl3
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui_glfw_opengl3 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_link_libraries(imgui_glfw_opengl3 PUBLIC glfw)
  if(APPLE)
    target_compile_definitions(imgui_glfw_opengl3 PUBLIC GL_SILENCE_DEPRECATION)
    target_compile_options(imgui_glfw_opengl3 PRIVATE -include OpenGL/gl3.h)
  endif()

  # OpenGL
  find_package(OpenGL REQUIRED)

  # Using system OpenGL headers on macOS; no dedicated loader target

  file(GLOB VIEWER_SOURCES
    src/*.cpp
  )
  add_executable(lidar_viewer ${VIEWER_SOURCES})
  target_include_directories(lidar_viewer PUBLIC src emulator)
  target_link_libraries(lidar_viewer PRIVATE imgui_glfw_opengl3 OpenGL::GL glm::glm)
  target_compile_definitions(lidar_viewer PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
  target_compile_definitions(imgui_glfw_opengl3 PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
  target_link_libraries(imgui_glfw_opengl3 PUBLIC OpenGL::GL)
  if(APPLE)
    target_compile_definitions(lidar_viewer PRIVATE GL_SILENCE_DEPRECATION)
  endif()
endif()

if(BUILD_EMULATOR)
  add_executable(rover_emulator emulator/rover_emulator.cpp)
  target_include_directories(rover_emulator PRIVATE emulator)
endif()


