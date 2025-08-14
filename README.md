# Multi-Rover LiDAR Visualization System

A real-time visualization system for multiple rover LiDAR data streams with interactive 3D rendering, comprehensive telemetry monitoring, and intuitive control interfaces.

## Quick Start

For first-time setup, run the automated setup script:

```bash
# Download dependencies and check system requirements
./setup.sh

# Build the project
make

# Start the system (requires two terminals)
make run-rovers    # Terminal 1: Start rover emulators
make run-app       # Terminal 2: Start visualization
```

The setup script automatically downloads and configures ImGui v1.89.9 and GLM, and checks for required system dependencies.

## Features

- **Real-time LiDAR Visualization**: Visualize point clouds from up to 5 rovers simultaneously
- **Interactive 3D Environment**: Navigate through the terrain with smooth camera controls
- **Comprehensive UI**: Monitor rover status, telemetry, and system diagnostics
- **High Performance**: Optimized for <50ms end-to-end latency with 60 FPS rendering
- **Network Management**: Robust UDP networking with automatic rover discovery
- **Error Recovery**: Graceful error handling and automatic reconnection
- **Multi-platform**: Supports Linux with OpenGL 3.3+

## System Architecture

The system consists of several integrated components:

- **Application Framework**: Main event loop and component coordination
- **Network Manager**: UDP communication handling for all rover data streams
- **Data Assembler**: LiDAR packet reconstruction and validation
- **Terrain Manager**: Point cloud storage and spatial indexing  
- **Renderer**: OpenGL-based 3D visualization
- **UI Manager**: ImGui-based user interface with multiple panels
- **Camera System**: 6DOF camera with smooth movement and controls

## Directory Structure
```
/project_root
├── src/                    # Main application source files
│   ├── main.cpp           # Application entry point
│   ├── Application.cpp    # Main application framework
│   ├── NetworkManager.cpp # Network communication
│   ├── DataAssembler.cpp  # LiDAR data processing
│   ├── TerrainManager.cpp # Point cloud management
│   ├── Renderer.cpp       # OpenGL rendering
│   ├── UIManager.cpp      # User interface management
│   ├── Camera.cpp         # Camera system
│   ├── PointCloudRenderer.cpp # Point cloud rendering
│   ├── RoverRenderer.cpp  # Rover visualization
│   └── panels/            # UI panel implementations
├── include/               # Header files
│   ├── Application.h      # Main application interface
│   ├── Constants.h        # System constants and configuration
│   └── panels/            # UI panel headers
├── emulator/              # Rover emulator (data source)
│   ├── rover_emulator.cpp # Emulator source
│   └── rover_profiles.h   # Rover configurations
├── shaders/               # GLSL shader files
│   ├── pointcloud.vert/.frag # Point cloud shaders
│   └── basic.vert/.frag   # Basic mesh shaders
├── external/              # External dependencies
│   ├── imgui/            # ImGui library (user provided)
│   └── glm/              # GLM math library (user provided)
├── data/                  # Rover data files
├── build/                 # Build output directory
├── docs/                  # Documentation
├── run_rovers.sh          # Rover emulator launcher
└── Makefile              # Build system
```

## Dependencies

### System Requirements
- Linux operating system (Ubuntu 18.04+ recommended)
- OpenGL 3.3+ compatible graphics card
- C++17 compatible compiler (GCC 7+ or Clang 6+)
- Minimum 4GB RAM, 8GB recommended
- 1GB free disk space

### Required Packages
Install the following packages on Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install libglfw3-dev libglew-dev libgl1-mesa-dev build-essential
```

For other distributions, install equivalent packages:
- **GLFW 3.3+**: Window management and input handling
- **GLEW**: OpenGL extension loading
- **OpenGL**: 3D rendering (usually provided by graphics drivers)

### External Libraries

The setup script automatically downloads and configures these libraries:

1. **ImGui v1.89.9**: GUI library
   - Automatically downloaded to: `external/imgui/`
   - Manual download: https://github.com/ocornut/imgui/releases

2. **GLM 0.9.9.8**: Mathematics library  
   - Automatically downloaded to: `external/glm/`
   - Manual download: https://github.com/g-truc/glm/releases

**Note**: The automated setup script handles all external dependencies. Manual download is only needed if you prefer not to use the setup script.

## Build Instructions

### Automated Setup (Recommended)
```bash
# Run the automated setup script (first time only)
./setup.sh

# Build everything (main application and emulator)
make

# Or build with specific configuration
make debug    # Debug build with symbols
make release  # Optimized release build
```

### Alternative Setup Methods
```bash
# Setup via Make
make setup

# Setup without system dependency checks (for CI/automated builds)
make setup-fast

# Force reinstall of external dependencies
make setup-force
```

### Manual Setup Process (Advanced)
If you prefer not to use the automated setup script:

```bash
# 1. Install system dependencies
sudo apt-get update
sudo apt-get install libglfw3-dev libglew-dev libgl1-mesa-dev build-essential

# 2. Manually download external libraries to external/ directory
# - ImGui v1.89.9 to external/imgui/
# - GLM 0.9.9.8 to external/glm/

# 3. Build the complete system
make all

# 4. Verify the build
make info  # Show build configuration
```

### Build Targets

**Main Targets:**
- `make` or `make all` - Build everything (default)
- `make debug` - Build with debug symbols and logging
- `make release` - Optimized production build

**Setup Targets:**
- `make setup` - Run automated setup script (recommended)
- `make setup-fast` - Setup without system dependency checks
- `make setup-force` - Force reinstall of external dependencies

**Utility Targets:**
- `make test-compile` - Test compilation without linking
- `make clean` - Remove build artifacts
- `make distclean` - Complete cleanup including external deps
- `make help` - Show all available targets

## Usage

### Running the Complete System

The system requires two components running simultaneously:

#### Terminal 1: Start Rover Emulators
```bash
# Start all 5 rover emulators with realistic noise
make run-rovers

# Or without noise for debugging
make run-rovers-noiseless

# Or manually
./run_rovers.sh
```

#### Terminal 2: Start Visualization Application
```bash
# Standard mode
make run-app

# Debug mode with verbose logging
make run-debug

# Fullscreen mode
make run-fullscreen

# Or with custom options
./lidar_viz --help
```

### Command Line Options

The main application supports various command line options:
```bash
./lidar_viz [options]

Options:
  --help, -h              Show help message
  --fullscreen            Start in fullscreen mode
  --windowed              Start in windowed mode (default)
  --width WIDTH           Set window width (default: 1280)
  --height HEIGHT         Set window height (default: 720)
  --fps FPS               Set target FPS (default: 60)
  --rovers COUNT          Set max rovers (default: 5)
  --no-vsync              Disable VSync
  --wireframe             Enable wireframe rendering
  --no-networking         Disable network connections
  --log FILE              Set log file path
  --no-file-log           Disable file logging
  --no-console-log        Disable console logging
  --debug                 Enable debug overlay
  --no-perf-stats         Disable performance statistics
```

### Controls

#### Camera Controls
- **Mouse + Right Click**: Look around (first-person style)
- **WASD**: Move forward/backward/left/right
- **Mouse Wheel**: Zoom in/out
- **Ctrl+R**: Reset camera to default position

#### Application Controls  
- **Escape**: Exit application
- **F1**: Toggle UI panels visibility
- **F2**: Toggle wireframe rendering mode
- **F3**: Toggle debug information overlay
- **F11**: Toggle fullscreen mode

#### UI Interaction
- **Rover Selection**: Click rover buttons to focus on specific rovers
- **Button Controls**: Send commands to rovers via UI buttons
- **View Options**: Customize rendering and display options
- **Performance Monitor**: View real-time system statistics

## Network Configuration

### Port Mapping
Each rover uses a set of predefined ports:

| Rover ID | Pose Data | LiDAR Data | Telemetry | Commands |
|----------|-----------|------------|-----------|----------|
| 1        | 9001      | 10001      | 11001     | 8001     |
| 2        | 9002      | 10002      | 11002     | 8002     |
| 3        | 9003      | 10003      | 11003     | 8003     |
| 4        | 9004      | 10004      | 11004     | 8004     |
| 5        | 9005      | 10005      | 11005     | 8005     |

All communication uses UDP on localhost (127.0.0.1) at 10Hz update rate.

### Data Formats
- **Pose Data**: Position (XYZ) and rotation (XYZ degrees) with timestamp
- **LiDAR Data**: Chunked point clouds with assembly metadata  
- **Telemetry**: Button states and system status
- **Commands**: Button control messages sent to rovers

## Performance

### Target Specifications
- **Frame Rate**: 60 FPS stable rendering
- **Latency**: <50ms end-to-end (network to display)
- **Point Capacity**: 500K total points displayed simultaneously
- **Memory Usage**: <2GB for typical operation
- **Network Throughput**: Supports full 5-rover data streams

### Optimization Features
- **Frustum Culling**: Only render visible points
- **Level of Detail**: Adaptive point density based on distance
- **Spatial Indexing**: Efficient point cloud queries
- **Multi-threading**: Separate threads for network, rendering, and UI
- **Memory Management**: Automatic cleanup and optimization

## Troubleshooting

### Build Issues
```bash
# Run automated setup (fixes most dependency issues)
./setup.sh

# Or force reinstall external dependencies
make setup-force

# Verify external libraries are present  
ls external/imgui external/glm

# Try clean rebuild
make clean && make debug

# Check detailed build output
make info
```

### Runtime Issues
```bash
# Check if rover emulators are running
ps aux | grep rover_emulator

# Verify network connectivity
netstat -an | grep "900[1-5]\|1000[1-5]"

# Run with debug output
./lidar_viz --debug

# Check log files
tail -f lidar_viz.log
```

### Performance Issues
- Reduce window size or disable VSync
- Lower target FPS with `--fps 30`
- Enable wireframe mode with F2
- Close unnecessary UI panels with F1
- Check system resources with debug overlay (F3)

### Common Problems

**"Failed to initialize renderer"**
- Ensure OpenGL 3.3+ drivers are installed
- Try running with `glxinfo | grep OpenGL` to check support

**"Network initialization failed"**  
- Check if ports are already in use
- Verify firewall isn't blocking localhost UDP traffic
- Try running without networking: `./lidar_viz --no-networking`

**"External library not found"**
- Run the automated setup script: `./setup.sh`
- Or force reinstall: `make setup-force`
- Verify directories exist: `ls external/imgui external/glm`

**Poor performance**
- Check if running on integrated graphics
- Monitor CPU/GPU usage in debug overlay
- Reduce point cloud display limits in UI

## Configuration

### System Constants
Key configuration values in `include/Constants.h`:

- **RenderConfig**: Frame rate, point limits, camera settings
- **NetworkConfig**: Timeouts, buffer sizes, connection parameters  
- **UIConfig**: Window dimensions, panel sizes, color themes
- **RoverConfig**: Rover limits, validation thresholds
- **DebugConfig**: Performance monitoring, debug visualization

### Runtime Configuration
The UI provides runtime configuration for:
- Point cloud visualization parameters
- Camera movement speeds
- Network connection settings  
- Rendering quality options
- Performance monitoring displays

## Development

### Adding New Features
1. Define interfaces in appropriate header files
2. Implement functionality in corresponding .cpp files
3. Update Makefile if adding new source files
4. Add configuration constants to Constants.h
5. Update UI panels as needed for user control

### Code Organization
- **Application Layer**: High-level coordination and lifecycle
- **Network Layer**: UDP communication and data parsing
- **Data Layer**: LiDAR assembly and terrain management
- **Render Layer**: OpenGL visualization and camera control
- **UI Layer**: ImGui interface and user interaction

### Testing
```bash
# Compile without linking to catch errors early
make test-compile

# Run with debug output to verify functionality  
make run-debug

# Test individual components by disabling others
./lidar_viz --no-networking  # Test rendering only
```

# Rover Emulator - Data Interface Specification

## **1. Network Ports**
Each rover sends:
- **Pose (Position/Orientation) Data** on **port `9000 + RoverID`**.
- **LiDAR (Point Cloud) Data** on **port `10000 + RoverID`**.

All traffic is **UDP** on **localhost (127.0.0.1)**.

**Example:**
- Rover **ID=1** → Pose on port `9001`, LiDAR on port `10001`.
- Rover **ID=2** → Pose on port `9002`, LiDAR on port `10002`.
- etc.
More details can be found in `emulator/rover_profiles.h`

---

## **2. Pose Packet Structure**
Each pose packet is a **binary** structure sent in a single UDP datagram:

```cpp
#pragma pack(push, 1)
struct PosePacket {
    double timestamp;    // Seconds since emulator start
    float  posX;         // X position (units)
    float  posY;         // Y position (units)
    float  posZ;         // Z position (units)
    float  rotXdeg;      // Roll or X rotation (degrees)
    float  rotYdeg;      // Pitch or Y rotation (degrees)
    float  rotZdeg;      // Yaw or Z rotation (degrees)
};
#pragma pack(pop)
```

- **Timestamp** is a `double` representing **seconds since the emulator started**.

---

## **3. LiDAR Packet Structure**
LiDAR data is **chunked** so it can fit within safe UDP packet sizes. Each LiDAR “scan” from the data file is broken into **N** chunks. Each chunk is a **binary** structure:

```cpp
static const size_t MAX_LIDAR_POINTS_PER_PACKET = 100;

#pragma pack(push, 1)
struct LidarPacketHeader {
    double   timestamp;       // Same as PosePacket timestamp
    uint32_t chunkIndex;      // Which chunk this is
    uint32_t totalChunks;     // Total chunks for this scan
    uint32_t pointsInThisChunk;
};

struct LidarPoint {
    float x;
    float y;
    float z;
};

struct LidarPacket {
    LidarPacketHeader header;
    LidarPoint points[MAX_LIDAR_POINTS_PER_PACKET];
};
#pragma pack(pop)
```

### **Transmission Details**
- Each chunk is sent in **one UDP datagram**.
- The actual bytes sent (`packetSize`) = `sizeof(LidarPacketHeader) + (pointsInThisChunk * sizeof(LidarPoint))`.
- **`chunkIndex`** runs from `0` to `totalChunks - 1`.
- **`pointsInThisChunk`** is how many points are in the current chunk.
- **`timestamp`** in the header matches the PosePacket timestamp for correlation.

**Example Calculation:**
- If a LiDAR scan has 350 points, it’s split into **4** chunks (3 full chunks of 100 points, and 1 chunk of 50 points).
- The receiver can reassemble the full point cloud for that timestamp by collecting chunks **0..3**.

---

## **4. Button State Control**
Each rover listens for **button state commands** and transmits the **current button states**.

### **4.1 Button Command Input**
- **Port:** `8000 + RoverID` (e.g., `8001` for rover `1`)
- **Format:** **1-byte (`uint8_t`) UDP message**, where each **bit** represents a **button state**:
  - **Bit 0** → Button 0 (1 = ON, 0 = OFF)
  - **Bit 1** → Button 1 (1 = ON, 0 = OFF)
  - **Bit 2** → Button 2 (1 = ON, 0 = OFF)
  - **Bit 3** → Button 3 (1 = ON, 0 = OFF)
- The rover updates **internal button states** upon receiving a command.

### **4.2 Button Telemetry Output**
- **Port:** `11000 + RoverID` (e.g., `11001` for rover `1`)
- **Sent at:** **10Hz** (same as pose & LiDAR)
- **Format:** `VehicleTelem` struct:
  
  ```cpp
  #pragma pack(push, 1)
  struct VehicleTelem {
      double timestamp;   // Same as PosePacket timestamp
      uint8_t buttonStates; // Bitfield for button states (4 bits used)
  };
  #pragma pack(pop)
  ```

- **Example:**
  - A `buttonStates` value of `0b00001001` (`9` in decimal) means:
    - **Button 0: ON**
    - **Button 1: OFF**
    - **Button 2: OFF**
    - **Button 3: ON**

---

## **Summary**
- **Four Ports per Rover**:
  - **Pose Data** (`9000 + RoverID`)
  - **LiDAR Data** (`10000 + RoverID`)
  - **Button Telemetry** (`11000 + RoverID`)
  - **Button Commands** (`8000 + RoverID`)
- **Pose**: Single struct with **timestamp**, position (XYZ), and rotation (XYZ in degrees).
- **LiDAR**: Multiple chunks per scan, each chunk includes a **header** plus an array of `(x,y,z)` points.
- **Buttons**: Receives **1-byte bitfield** commands and reports **1-byte bitfield** telemetry.
- **All traffic** is **sent via UDP** to **localhost** at a **10 Hz** rate.
