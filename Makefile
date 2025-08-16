# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17

# Directories
SRC_DIR := emulator
BUILD_DIR := .
DATA_DIR := data
BIN_DIR := build

# Source files
SRCS := $(SRC_DIR)/rover_emulator.cpp
HDRS := $(SRC_DIR)/rover_profiles.h
TARGET := $(BUILD_DIR)/rover_emulator

# Default rule: build the emulator
all: $(TARGET) extract

# Compile the main executable
$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

# Provide the binary where run script expects it
$(BIN_DIR)/rover_emulator: $(TARGET)
	@mkdir -p $(BIN_DIR)
	cp $(TARGET) $(BIN_DIR)/rover_emulator
	chmod +x $(BIN_DIR)/rover_emulator

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

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Remove CMake build directory and copied binaries
clean-build:
	rm -rf $(BIN_DIR)

# Configure and build via CMake (builds viewer and emulator into build/)
cmake-configure:
	@mkdir -p $(BIN_DIR)
	cmake -S . -B $(BIN_DIR)

cmake-build: cmake-configure
	cmake --build $(BIN_DIR) --parallel

# Run the OpenGL/ImGui viewer
run-viewer: cmake-build
	./$(BIN_DIR)/lidar_viewer

# Runs all rover emulators (ensures binary exists where script expects it)
run: extract $(BIN_DIR)/rover_emulator
	./run_rovers.sh

run-noiseless: extract $(BIN_DIR)/rover_emulator
	./run_rovers.sh --no-noise

.PHONY: all clean clean-build extract cmake-configure cmake-build run run-noiseless run-viewer
