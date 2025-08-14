#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstddef>

/**
 * Network configuration constants
 */
namespace NetworkConfig {
    // Network timeouts and intervals
    static constexpr double ROVER_OFFLINE_TIMEOUT_MS = 5000.0;  // 5 seconds
    static constexpr double PACKET_TIMEOUT_MS = 200.0;          // 200ms for LiDAR assembly
    static constexpr double HEARTBEAT_INTERVAL_MS = 1000.0;     // 1 second heartbeat
    static constexpr int NETWORK_UPDATE_INTERVAL_MS = 16;       // ~60 FPS network updates
    
    // Buffer sizes
    static constexpr size_t UDP_BUFFER_SIZE = 65536;            // 64KB UDP receive buffer
    static constexpr size_t MAX_LIDAR_ASSEMBLY_SCANS = 10;      // Max concurrent LiDAR scans per rover
    static constexpr size_t MAX_COMMAND_QUEUE_SIZE = 100;       // Max queued commands
    
    // Network addresses
    static constexpr const char* LOOPBACK_ADDR = "127.0.0.1";
    static constexpr const char* MULTICAST_ADDR = "224.0.0.1";
    
    // Socket options
    static constexpr int SOCKET_RECV_TIMEOUT_MS = 100;          // Socket receive timeout
    static constexpr int SO_RCVBUF_SIZE = 262144;               // 256KB socket receive buffer
    static constexpr int SO_SNDBUF_SIZE = 65536;                // 64KB socket send buffer
}

/**
 * Rendering and visualization constants
 */
namespace RenderConfig {
    // Frame rate and timing
    static constexpr int TARGET_FPS = 60;
    static constexpr double FRAME_TIME_MS = 1000.0 / TARGET_FPS;
    static constexpr int MAX_FRAME_SKIP = 5;                    // Max frames to skip if behind
    
    // Point cloud limits
    static constexpr size_t MAX_POINTS_PER_ROVER = 100000;      // 100K points per rover
    static constexpr size_t MAX_TOTAL_POINTS = 500000;          // 500K total points displayed
    static constexpr size_t POINT_BUFFER_SIZE = 1000000;        // 1M point buffer size
    
    // Culling and optimization
    static constexpr float FRUSTUM_CULLING_MARGIN = 10.0f;      // Frustum culling margin in meters
    static constexpr float LOD_DISTANCE_NEAR = 50.0f;           // Near LOD distance
    static constexpr float LOD_DISTANCE_FAR = 200.0f;           // Far LOD distance
    static constexpr float POINT_SIZE_MIN = 1.0f;               // Minimum point size
    static constexpr float POINT_SIZE_MAX = 5.0f;               // Maximum point size
    
    // Camera defaults
    static constexpr float DEFAULT_FOV = 60.0f;                 // Field of view in degrees
    static constexpr float NEAR_PLANE = 0.1f;                   // Near clipping plane
    static constexpr float FAR_PLANE = 1000.0f;                 // Far clipping plane
    
    // Movement speeds
    static constexpr float CAMERA_MOVE_SPEED = 10.0f;           // m/s
    static constexpr float CAMERA_ROTATE_SPEED = 90.0f;         // degrees/s
    static constexpr float CAMERA_ZOOM_SPEED = 2.0f;            // zoom multiplier
}

/**
 * User interface constants
 */
namespace UIConfig {
    // Window defaults
    static constexpr int DEFAULT_WINDOW_WIDTH = 1280;
    static constexpr int DEFAULT_WINDOW_HEIGHT = 720;
    static constexpr int MIN_WINDOW_WIDTH = 800;
    static constexpr int MIN_WINDOW_HEIGHT = 600;
    
    // Panel sizes
    static constexpr int SIDEBAR_WIDTH = 300;                   // Right sidebar width
    static constexpr int TOOLBAR_HEIGHT = 40;                   // Top toolbar height
    static constexpr int STATUS_BAR_HEIGHT = 25;                // Bottom status bar height
    
    // Update intervals
    static constexpr int UI_UPDATE_INTERVAL_MS = 33;            // ~30 FPS UI updates
    static constexpr int STATUS_UPDATE_INTERVAL_MS = 500;       // Status updates every 500ms
    
    // Text and fonts
    static constexpr float FONT_SIZE_NORMAL = 14.0f;
    static constexpr float FONT_SIZE_SMALL = 12.0f;
    static constexpr float FONT_SIZE_LARGE = 16.0f;
    static constexpr float FONT_SIZE_HEADER = 18.0f;
    
    // Colors (RGBA, 0-255)
    struct Colors {
        static constexpr int BACKGROUND[4] = {45, 45, 48, 255};     // Dark gray background
        static constexpr int PANEL[4] = {60, 60, 63, 255};          // Panel background
        static constexpr int TEXT[4] = {255, 255, 255, 255};        // White text
        static constexpr int TEXT_DISABLED[4] = {128, 128, 128, 255}; // Gray disabled text
        static constexpr int ACCENT[4] = {0, 122, 255, 255};        // Blue accent
        static constexpr int SUCCESS[4] = {52, 199, 89, 255};       // Green success
        static constexpr int WARNING[4] = {255, 214, 10, 255};      // Yellow warning
        static constexpr int ERROR[4] = {255, 69, 58, 255};         // Red error
    };
    
    // Control sizes
    static constexpr int BUTTON_HEIGHT = 28;
    static constexpr int INPUT_HEIGHT = 24;
    static constexpr int SLIDER_HEIGHT = 20;
    static constexpr int CHECKBOX_SIZE = 16;
    
    // Spacing
    static constexpr int PADDING_SMALL = 4;
    static constexpr int PADDING_NORMAL = 8;
    static constexpr int PADDING_LARGE = 16;
    static constexpr int SPACING_SMALL = 4;
    static constexpr int SPACING_NORMAL = 8;
    static constexpr int SPACING_LARGE = 16;
}

/**
 * Rover and telemetry constants
 */
namespace RoverConfig {
    // Physical limits
    static constexpr int MAX_ROVERS = 10;                       // Maximum supported rovers
    static constexpr int MIN_ROVER_ID = 1;                      // Minimum rover ID
    static constexpr int MAX_ROVER_ID = MAX_ROVERS;             // Maximum rover ID
    
    // Button system
    static constexpr int BUTTON_COUNT = 4;                      // Number of buttons per rover
    static constexpr uint8_t BUTTON_1_MASK = 0x01;             // Button 1 bit mask
    static constexpr uint8_t BUTTON_2_MASK = 0x02;             // Button 2 bit mask
    static constexpr uint8_t BUTTON_3_MASK = 0x04;             // Button 3 bit mask
    static constexpr uint8_t BUTTON_4_MASK = 0x08;             // Button 4 bit mask
    static constexpr uint8_t ALL_BUTTONS_MASK = 0x0F;          // All buttons mask
    
    // Data validation
    static constexpr double MAX_VALID_TIMESTAMP = 1e15;         // Max valid timestamp (to detect corruption)
    static constexpr float MAX_COORDINATE_VALUE = 10000.0f;     // Max coordinate value (meters)
    static constexpr float MAX_ROTATION_DEGREES = 360.0f;       // Max rotation value
    
    // Performance limits
    static constexpr size_t MAX_LIDAR_POINTS_PER_SCAN = 50000;  // Max points in a complete scan
    static constexpr double MAX_SCAN_DURATION_MS = 1000.0;      // Max time for a complete scan
}

/**
 * File and data constants
 */
namespace FileConfig {
    // File paths
    static constexpr const char* DEFAULT_LOG_FILE = "lidar_viz.log";
    static constexpr const char* CONFIG_FILE = "config.ini";
    static constexpr const char* CACHE_DIRECTORY = "cache/";
    
    // File limits
    static constexpr size_t MAX_LOG_FILE_SIZE = 10 * 1024 * 1024;  // 10MB max log file
    static constexpr size_t MAX_DATA_FILE_SIZE = 100 * 1024 * 1024; // 100MB max data file
    
    // Buffer sizes for file I/O
    static constexpr size_t FILE_READ_BUFFER_SIZE = 8192;           // 8KB read buffer
    static constexpr size_t FILE_WRITE_BUFFER_SIZE = 8192;          // 8KB write buffer
}

/**
 * Debug and diagnostics constants
 */
namespace DebugConfig {
    // Debug visualization
    static constexpr bool SHOW_DEBUG_INFO = true;              // Show debug overlay
    static constexpr bool SHOW_PERFORMANCE_STATS = true;       // Show performance metrics
    static constexpr bool SHOW_NETWORK_STATS = true;           // Show network statistics
    
    // Performance monitoring
    static constexpr int PERF_HISTORY_SIZE = 300;              // 5 seconds at 60 FPS
    static constexpr double PERF_WARNING_THRESHOLD = 33.0;     // Warn if frame time > 33ms
    static constexpr double PERF_CRITICAL_THRESHOLD = 66.0;    // Critical if frame time > 66ms
    
    // Memory monitoring
    static constexpr size_t MEMORY_WARNING_THRESHOLD = 1024ULL * 1024ULL * 1024ULL; // 1GB warning
    static constexpr size_t MEMORY_CRITICAL_THRESHOLD = 2048ULL * 1024ULL * 1024ULL; // 2GB critical
}

#endif // CONSTANTS_H