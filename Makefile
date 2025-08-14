# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17

# Directories
SRC_DIR := emulator
BUILD_DIR := .
DATA_DIR := data

# Source files
SRCS := $(SRC_DIR)/rover_emulator.cpp
HDRS := $(SRC_DIR)/rover_profiles.h
TARGET := $(BUILD_DIR)/rover_emulator

# Default rule: build the emulator
all: $(TARGET) extract

# Compile the main executable
$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

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

# Runs all rover emulators
run: extract
	./run_rovers.sh

run-noiseless: extract
	./run_rovers.sh --no-noise

.PHONY: all clean run
