#!/bin/bash

# Multi-Rover LiDAR Visualization System - Setup Script
# This script automatically downloads and configures ImGui and checks system dependencies

set -e  # Exit on any error

# Color output for better visibility
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
IMGUI_VERSION="v1.89.9"
IMGUI_URL="https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.tar.gz"
GLM_VERSION="0.9.9.8"
GLM_URL="https://github.com/g-truc/glm/releases/download/${GLM_VERSION}/glm-${GLM_VERSION}.zip"

EXTERNAL_DIR="external"
IMGUI_DIR="${EXTERNAL_DIR}/imgui"
GLM_DIR="${EXTERNAL_DIR}/glm"

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}================================================${NC}"
    echo -e "${BLUE} Multi-Rover LiDAR Visualization System Setup${NC}"
    echo -e "${BLUE}================================================${NC}\n"
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check system dependencies
check_system_dependencies() {
    print_status "Checking system dependencies..."
    
    local missing_deps=()
    
    # Check for essential build tools
    if ! command_exists gcc && ! command_exists clang; then
        missing_deps+=("gcc/clang (C++ compiler)")
    fi
    
    if ! command_exists make; then
        missing_deps+=("make")
    fi
    
    if ! command_exists pkg-config; then
        missing_deps+=("pkg-config")
    fi
    
    # Check for required libraries using pkg-config
    local required_libs=("glfw3" "glew" "gl")
    for lib in "${required_libs[@]}"; do
        if ! pkg-config --exists "$lib" 2>/dev/null; then
            missing_deps+=("lib${lib}-dev")
        fi
    done
    
    # Check for download tools
    if ! command_exists curl && ! command_exists wget; then
        missing_deps+=("curl or wget")
    fi
    
    if ! command_exists tar; then
        missing_deps+=("tar")
    fi
    
    if ! command_exists unzip; then
        missing_deps+=("unzip")
    fi
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        print_success "All system dependencies are satisfied!"
        return 0
    else
        print_error "Missing system dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo
        print_status "To install missing dependencies on Ubuntu/Debian:"
        echo "  sudo apt-get update"
        echo "  sudo apt-get install build-essential pkg-config libglfw3-dev libglew-dev libgl1-mesa-dev curl unzip"
        echo
        print_status "On CentOS/RHEL/Fedora:"
        echo "  sudo yum install gcc-c++ make pkgconfig glfw-devel glew-devel mesa-libGL-devel curl unzip"
        echo "  # or on newer Fedora: sudo dnf install ..."
        echo
        print_status "On Arch Linux:"
        echo "  sudo pacman -S base-devel glfw-wayland glew mesa curl unzip"
        echo
        return 1
    fi
}

# Create directory structure
create_directories() {
    print_status "Creating directory structure..."
    mkdir -p "$EXTERNAL_DIR"
    print_success "Created external directory structure"
}

# Download and extract ImGui
setup_imgui() {
    print_status "Setting up ImGui ${IMGUI_VERSION}..."
    
    if [ -d "$IMGUI_DIR" ]; then
        print_warning "ImGui directory already exists. Checking version..."
        if [ -f "$IMGUI_DIR/imgui.h" ]; then
            # Try to extract version from header
            local existing_version=$(grep -E '#define IMGUI_VERSION_NUM' "$IMGUI_DIR/imgui.h" 2>/dev/null || echo "unknown")
            print_status "Found existing ImGui installation: $existing_version"
            read -p "Overwrite existing ImGui installation? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                print_status "Skipping ImGui setup"
                return 0
            fi
            rm -rf "$IMGUI_DIR"
        fi
    fi
    
    mkdir -p "$IMGUI_DIR"
    
    # Download ImGui
    print_status "Downloading ImGui ${IMGUI_VERSION}..."
    local temp_file="/tmp/imgui-${IMGUI_VERSION}.tar.gz"
    
    if command_exists curl; then
        curl -L "$IMGUI_URL" -o "$temp_file" || {
            print_error "Failed to download ImGui with curl"
            return 1
        }
    elif command_exists wget; then
        wget "$IMGUI_URL" -O "$temp_file" || {
            print_error "Failed to download ImGui with wget"
            return 1
        }
    else
        print_error "Neither curl nor wget available for downloading"
        return 1
    fi
    
    # Extract ImGui
    print_status "Extracting ImGui..."
    tar -xzf "$temp_file" -C "$IMGUI_DIR" --strip-components=1 || {
        print_error "Failed to extract ImGui"
        rm -f "$temp_file"
        return 1
    }
    
    rm -f "$temp_file"
    
    # Verify essential ImGui files exist
    local essential_files=("imgui.h" "imgui.cpp" "backends/imgui_impl_glfw.cpp" "backends/imgui_impl_opengl3.cpp")
    for file in "${essential_files[@]}"; do
        if [ ! -f "$IMGUI_DIR/$file" ]; then
            print_error "Missing essential ImGui file: $file"
            return 1
        fi
    done
    
    print_success "ImGui ${IMGUI_VERSION} installed successfully"
}

# Download and extract GLM
setup_glm() {
    print_status "Setting up GLM ${GLM_VERSION}..."
    
    if [ -d "$GLM_DIR" ]; then
        print_warning "GLM directory already exists. Checking version..."
        if [ -f "$GLM_DIR/glm/glm.hpp" ]; then
            # Try to extract version from header
            local existing_version=$(grep -E '#define GLM_VERSION' "$GLM_DIR/glm/detail/setup.hpp" 2>/dev/null | head -1 || echo "unknown")
            print_status "Found existing GLM installation: $existing_version"
            read -p "Overwrite existing GLM installation? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                print_status "Skipping GLM setup"
                return 0
            fi
            rm -rf "$GLM_DIR"
        fi
    fi
    
    mkdir -p "$GLM_DIR"
    
    # Download GLM
    print_status "Downloading GLM ${GLM_VERSION}..."
    local temp_file="/tmp/glm-${GLM_VERSION}.zip"
    
    if command_exists curl; then
        curl -L "$GLM_URL" -o "$temp_file" || {
            print_error "Failed to download GLM with curl"
            return 1
        }
    elif command_exists wget; then
        wget "$GLM_URL" -O "$temp_file" || {
            print_error "Failed to download GLM with wget"
            return 1
        }
    else
        print_error "Neither curl nor wget available for downloading"
        return 1
    fi
    
    # Extract GLM
    print_status "Extracting GLM..."
    unzip -q "$temp_file" -d "/tmp/" || {
        print_error "Failed to extract GLM"
        rm -f "$temp_file"
        return 1
    }
    
    # Move GLM contents (handle different archive structures)
    if [ -d "/tmp/glm" ]; then
        cp -r /tmp/glm/* "$GLM_DIR/"
        rm -rf /tmp/glm
    elif [ -d "/tmp/glm-${GLM_VERSION}" ]; then
        cp -r "/tmp/glm-${GLM_VERSION}"/* "$GLM_DIR/"
        rm -rf "/tmp/glm-${GLM_VERSION}"
    else
        print_error "Unexpected GLM archive structure"
        rm -f "$temp_file"
        return 1
    fi
    
    rm -f "$temp_file"
    
    # Verify essential GLM files exist
    if [ ! -f "$GLM_DIR/glm/glm.hpp" ]; then
        print_error "Missing essential GLM file: glm/glm.hpp"
        return 1
    fi
    
    print_success "GLM ${GLM_VERSION} installed successfully"
}

# Verify installation
verify_installation() {
    print_status "Verifying installation..."
    
    local all_good=true
    
    # Check ImGui
    if [ -f "$IMGUI_DIR/imgui.h" ] && [ -f "$IMGUI_DIR/backends/imgui_impl_glfw.cpp" ] && [ -f "$IMGUI_DIR/backends/imgui_impl_opengl3.cpp" ]; then
        print_success "ImGui installation verified"
    else
        print_error "ImGui installation incomplete"
        all_good=false
    fi
    
    # Check GLM
    if [ -f "$GLM_DIR/glm/glm.hpp" ]; then
        print_success "GLM installation verified"
    else
        print_error "GLM installation incomplete"
        all_good=false
    fi
    
    if [ "$all_good" = true ]; then
        print_success "All external dependencies installed and verified!"
        return 0
    else
        return 1
    fi
}

# Test compilation
test_compile() {
    print_status "Testing compilation..."
    
    # Create a simple test file
    local test_file="/tmp/imgui_test.cpp"
    cat > "$test_file" << 'EOF'
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main() {
    // Test ImGui
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::DestroyContext(ctx);
    
    // Test GLM
    glm::mat4 matrix = glm::mat4(1.0f);
    glm::vec3 vector = glm::vec3(1.0f, 0.0f, 0.0f);
    
    return 0;
}
EOF

    # Try to compile the test
    local include_paths="-I$IMGUI_DIR -I$IMGUI_DIR/backends -I$GLM_DIR"
    if command_exists g++; then
        if g++ -std=c++17 $include_paths "$test_file" -o /tmp/imgui_test 2>/dev/null; then
            print_success "Compilation test passed"
            rm -f /tmp/imgui_test "$test_file"
            return 0
        else
            print_warning "Compilation test failed, but this may be due to missing system libraries"
        fi
    else
        print_warning "No C++ compiler found for compilation test"
    fi
    
    rm -f /tmp/imgui_test "$test_file"
    return 0  # Don't fail the setup due to compilation test issues
}

# Main setup function
main() {
    print_header
    
    # Parse command line arguments
    local skip_deps=false
    local force=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-deps)
                skip_deps=true
                shift
                ;;
            --force)
                force=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [options]"
                echo "Options:"
                echo "  --skip-deps    Skip system dependency checking"
                echo "  --force        Force reinstallation of external libraries"
                echo "  -h, --help     Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Check system dependencies unless skipped
    if [ "$skip_deps" = false ]; then
        if ! check_system_dependencies; then
            print_error "System dependencies check failed. Install missing packages and run again."
            print_status "You can skip this check with: $0 --skip-deps"
            exit 1
        fi
    else
        print_warning "Skipping system dependency check as requested"
    fi
    
    # Create directories
    create_directories
    
    # Setup external libraries
    if [ "$force" = true ]; then
        print_status "Force mode enabled - will reinstall all external libraries"
        rm -rf "$IMGUI_DIR" "$GLM_DIR"
    fi
    
    setup_imgui || {
        print_error "Failed to setup ImGui"
        exit 1
    }
    
    setup_glm || {
        print_error "Failed to setup GLM"
        exit 1
    }
    
    # Verify installation
    verify_installation || {
        print_error "Installation verification failed"
        exit 1
    }
    
    # Test compilation
    test_compile
    
    # Success message
    echo
    print_success "Setup completed successfully!"
    echo
    print_status "You can now build the project with:"
    echo "  make"
    echo "  make debug    # For debug build"
    echo "  make release  # For release build"
    echo
    print_status "To run the system:"
    echo "  make run-rovers    # Terminal 1: Start rover emulators"
    echo "  make run-app       # Terminal 2: Start visualization"
    echo
    print_status "For help with build targets:"
    echo "  make help"
}

# Run main function
main "$@"