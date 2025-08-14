# Multi-Rover LiDAR Visualization System Makefile

# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17 -DIMGUI_IMPL_OPENGL_ES2
DEBUG_FLAGS := -g -DDEBUG -O0
RELEASE_FLAGS := -O3 -DNDEBUG

# Directories
SRC_DIR := src
EMULATOR_DIR := emulator
INCLUDE_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DATA_DIR := data
EXTERNAL_DIR := external

# External library paths (adjust these for your system)
IMGUI_DIR := $(EXTERNAL_DIR)/imgui
GLM_INCLUDE := $(EXTERNAL_DIR)/glm

# System libraries
LIBS := -lGL -lGLEW -lglfw -lpthread -ldl -lm
INCLUDES := -I$(INCLUDE_DIR) -I$(IMGUI_DIR) -I$(GLM_INCLUDE) -I$(IMGUI_DIR)/backends

# Source files for main application
APP_SOURCES := $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)/panels/*.cpp)
APP_HEADERS := $(wildcard $(INCLUDE_DIR)/*.h) $(wildcard $(INCLUDE_DIR)/panels/*.h)
APP_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(APP_SOURCES))

# ImGui source files
IMGUI_SOURCES := $(IMGUI_DIR)/imgui.cpp \
                 $(IMGUI_DIR)/imgui_demo.cpp \
                 $(IMGUI_DIR)/imgui_draw.cpp \
                 $(IMGUI_DIR)/imgui_tables.cpp \
                 $(IMGUI_DIR)/imgui_widgets.cpp \
                 $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
                 $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

IMGUI_OBJECTS := $(patsubst $(IMGUI_DIR)/%.cpp,$(OBJ_DIR)/imgui_%.o,$(IMGUI_SOURCES))

# Emulator files
EMULATOR_SOURCES := $(EMULATOR_DIR)/rover_emulator.cpp
EMULATOR_HEADERS := $(EMULATOR_DIR)/rover_profiles.h
EMULATOR_TARGET := rover_emulator

# Main application target
MAIN_TARGET := lidar_viz

# All object files
ALL_OBJECTS := $(APP_OBJECTS) $(IMGUI_OBJECTS)

# Default rule: build everything
all: check-dependencies directories $(MAIN_TARGET) $(EMULATOR_TARGET) extract

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: check-dependencies directories $(MAIN_TARGET) $(EMULATOR_TARGET) extract

# Release build  
release: CXXFLAGS += $(RELEASE_FLAGS)
release: check-dependencies directories $(MAIN_TARGET) $(EMULATOR_TARGET) extract

# Check for external dependencies
check-dependencies:
	@if [ ! -d "$(IMGUI_DIR)" ] || [ ! -f "$(IMGUI_DIR)/imgui.h" ]; then \
		echo "❌ ImGui not found in $(IMGUI_DIR)"; \
		echo ""; \
		echo "Run the setup script to automatically download and configure dependencies:"; \
		echo "  ./setup.sh"; \
		echo ""; \
		echo "Or manually run: make setup"; \
		echo ""; \
		exit 1; \
	fi
	@if [ ! -d "$(GLM_INCLUDE)" ] || [ ! -f "$(GLM_INCLUDE)/glm/glm.hpp" ]; then \
		echo "❌ GLM not found in $(GLM_INCLUDE)"; \
		echo ""; \
		echo "Run the setup script to automatically download and configure dependencies:"; \
		echo "  ./setup.sh"; \
		echo ""; \
		echo "Or manually run: make setup"; \
		echo ""; \
		exit 1; \
	fi
	@echo "✅ All external dependencies found"

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/panels
	@mkdir -p $(OBJ_DIR)/imgui
	@mkdir -p $(OBJ_DIR)/imgui/backends

# Main application
$(MAIN_TARGET): $(ALL_OBJECTS)
	@echo "Linking main application..."
	$(CXX) $(CXXFLAGS) $(ALL_OBJECTS) -o $@ $(LIBS)
	@echo "Built main application: $(MAIN_TARGET)"

# Application object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(APP_HEADERS)
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ImGui object files
$(OBJ_DIR)/imgui_%.o: $(IMGUI_DIR)/%.cpp
	@echo "Compiling ImGui: $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ImGui backends
$(OBJ_DIR)/imgui_backends_%.o: $(IMGUI_DIR)/backends/%.cpp
	@echo "Compiling ImGui backend: $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Emulator (keep original functionality)
$(EMULATOR_TARGET): $(EMULATOR_SOURCES) $(EMULATOR_HEADERS)
	@echo "Building rover emulator..."
	$(CXX) $(CXXFLAGS) $(EMULATOR_SOURCES) -o $@

# Extract .dat files only if they don't already exist
extract:
	@for archive in data/*.tar.xz; do \
		extracted_file=$$(basename $$archive .tar.xz); \
		if [ ! -f "data/$$extracted_file" ]; then \
			echo "Extracting $$archive..."; \
			tar -xJf $$archive -C data/ --strip-components=1; \
		else \
			echo "$$extracted_file already exists. Skipping extraction."; \
		fi \
	done

# Utility targets
setup: directories
	@echo "Running automated setup script..."
	@echo ""
	./setup.sh

# Install target for setting up external dependencies (legacy - use setup instead)
install-deps:
	@echo "DEPRECATED: Use 'make setup' or './setup.sh' instead"
	@echo ""
	@echo "Running automated setup script..."
	./setup.sh

# Quick setup without dependency checking (for CI/automated builds)
setup-fast:
	@echo "Running setup script without system dependency checks..."
	./setup.sh --skip-deps

# Force reinstall of external dependencies
setup-force:
	@echo "Running setup script with force reinstall..."
	./setup.sh --force

# Clean build artifacts  
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(MAIN_TARGET)
	rm -f $(EMULATOR_TARGET)
	@echo "Clean complete"

# Deep clean (including external dependencies)
distclean: clean
	rm -rf $(EXTERNAL_DIR)
	@echo "Deep clean complete"

# Run the main application
run-app: $(MAIN_TARGET)
	@echo "Starting LiDAR Visualization Application..."
	./$(MAIN_TARGET)

# Run with debug output
run-debug: debug
	@echo "Starting LiDAR Visualization Application (debug mode)..."
	./$(MAIN_TARGET) --debug

# Run in fullscreen mode
run-fullscreen: $(MAIN_TARGET)
	@echo "Starting LiDAR Visualization Application (fullscreen)..."
	./$(MAIN_TARGET) --fullscreen

# Run rover emulators
run-rovers: $(EMULATOR_TARGET) extract
	@echo "Starting rover emulators..."
	./run_rovers.sh

# Run rover emulators without noise
run-rovers-noiseless: $(EMULATOR_TARGET) extract  
	@echo "Starting rover emulators (no noise)..."
	./run_rovers.sh --no-noise

# Run both application and emulators (requires two terminals)
run-all:
	@echo "To run the complete system:"
	@echo "1. In this terminal, run: make run-rovers"
	@echo "2. In another terminal, run: make run-app"
	@echo ""
	@echo "Or use the following commands in separate terminals:"
	@echo "  Terminal 1: ./run_rovers.sh"
	@echo "  Terminal 2: ./$(MAIN_TARGET)"

# Test build (compile only, don't link)
test-compile: directories $(APP_OBJECTS)
	@echo "Test compilation successful"

# Show build information
info:
	@echo "Multi-Rover LiDAR Visualization System"
	@echo "======================================"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Includes: $(INCLUDES)"
	@echo "Libraries: $(LIBS)"
	@echo "Source files: $(words $(APP_SOURCES)) application sources"
	@echo "Header files: $(words $(APP_HEADERS)) application headers"
	@echo "ImGui sources: $(words $(IMGUI_SOURCES)) files"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Main target: $(MAIN_TARGET)"
	@echo "Emulator target: $(EMULATOR_TARGET)"

# Help target
help:
	@echo "Multi-Rover LiDAR Visualization System - Build Help"
	@echo "================================================="
	@echo ""
	@echo "Main targets:"
	@echo "  all          - Build everything (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized release version"
	@echo "  clean        - Clean build artifacts"
	@echo "  distclean    - Clean everything including external deps"
	@echo ""
	@echo "Setup targets:"
	@echo "  setup        - Run automated setup script (recommended)"
	@echo "  setup-fast   - Setup without system dependency checks"
	@echo "  setup-force  - Force reinstall of external dependencies"
	@echo "  install-deps - Legacy setup target (deprecated)"
	@echo ""
	@echo "Run targets:"
	@echo "  run-app      - Run the visualization application"
	@echo "  run-debug    - Run application with debug output"
	@echo "  run-fullscreen - Run application in fullscreen"
	@echo "  run-rovers   - Run rover emulators"
	@echo "  run-rovers-noiseless - Run emulators without noise"
	@echo "  run-all      - Instructions to run complete system"
	@echo ""
	@echo "Utility targets:"
	@echo "  test-compile - Test compilation without linking"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "  make setup              # Run automated setup (first time)"
	@echo "  make                    # Build everything"
	@echo "  make run-rovers &       # Start emulators in background"
	@echo "  make run-app            # Start visualization"
	@echo ""
	@echo "First-time setup:"
	@echo "  ./setup.sh              # Automated setup script"
	@echo "  make setup              # Same as above via Make"

.PHONY: all debug release check-dependencies directories setup setup-fast setup-force \
        install-deps clean distclean run-app run-debug run-fullscreen run-rovers \
        run-rovers-noiseless run-all test-compile info help extract
