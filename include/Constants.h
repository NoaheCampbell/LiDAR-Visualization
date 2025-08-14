#pragma once

#include <cstdint>
#include <chrono>

/**
 * System-wide constants for Multi-Rover LiDAR Visualization
 * Performance targets and timeouts as specified in PRD
 */

namespace Constants {
    // Performance Targets (in milliseconds)
    namespace Performance {
        constexpr uint32_t END_TO_END_LATENCY_MS = 50;    // Total system latency target
        constexpr uint32_t NETWORK_POLLING_MS = 5;        // Network polling interval
        constexpr uint32_t LIDAR_ASSEMBLY_MS = 5;         // LiDAR data assembly time
        constexpr uint32_t RENDERING_MS = 16;             // Rendering frame time (60 FPS)
        constexpr uint32_t UI_UPDATE_MS = 4;              // UI update time
        constexpr uint32_t TOTAL_FRAME_BUDGET_MS = 33;    // Total frame budget (30 FPS minimum)
        
        // Timeout values
        constexpr uint32_t LIDAR_TIMEOUT_MS = 200;        // Timeout for incomplete LiDAR scans
        constexpr uint32_t ROVER_DISCONNECT_TIMEOUT_MS = 1000; // Rover disconnect detection
    }
    
    // Network Configuration
    namespace Network {
        constexpr size_t MAX_ROVERS = 5;                  // Maximum number of supported rovers
        constexpr size_t UDP_BUFFER_SIZE = 65536;         // UDP receive buffer size
        constexpr size_t MAX_PACKET_SIZE = 1472;          // Maximum UDP packet size (MTU - headers)
        constexpr uint32_t SOCKET_TIMEOUT_MS = 10;        // Socket receive timeout
        constexpr size_t NETWORK_THREAD_POOL_SIZE = 4;    // Number of network worker threads
    }
    
    // LiDAR Data Configuration
    namespace LiDAR {
        constexpr size_t MAX_POINTS_PER_SCAN = 10000;     // Maximum points in a complete scan
        constexpr size_t MAX_POINTS_PER_PACKET = 100;     // Points per UDP packet
        constexpr size_t POINT_CLOUD_HISTORY_SIZE = 10;   // Number of historical scans to keep
        constexpr float MAX_RANGE_METERS = 50.0f;         // Maximum LiDAR range for filtering
        constexpr float MIN_RANGE_METERS = 0.1f;          // Minimum LiDAR range for filtering
    }
    
    // Rendering Configuration
    namespace Rendering {
        constexpr uint32_t DEFAULT_WINDOW_WIDTH = 1280;
        constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 720;
        constexpr uint32_t MSAA_SAMPLES = 4;              // Anti-aliasing samples
        constexpr float FOV_DEGREES = 60.0f;              // Field of view
        constexpr float NEAR_PLANE = 0.1f;                // Near clipping plane
        constexpr float FAR_PLANE = 1000.0f;              // Far clipping plane
        
        // Point cloud rendering
        constexpr float DEFAULT_POINT_SIZE = 2.0f;
        constexpr float MAX_POINT_SIZE = 10.0f;
        constexpr float MIN_POINT_SIZE = 1.0f;
    }
    
    // UI Configuration
    namespace UI {
        constexpr uint32_t DIAGNOSTICS_UPDATE_INTERVAL_MS = 100; // Diagnostics panel update rate
        constexpr uint32_t METRICS_HISTORY_SIZE = 1000;    // Number of metrics samples to keep
        constexpr float PANEL_ALPHA = 0.9f;                // UI panel transparency
    }
    
    // File Paths
    namespace Paths {
        constexpr const char* SHADER_DIRECTORY = "shaders/";
        constexpr const char* LOG_DIRECTORY = "logs/";
        constexpr const char* CONFIG_FILE = "config.json";
    }
    
    // Rover Identification
    namespace Rover {
        constexpr uint8_t INVALID_ROVER_ID = 255;
        constexpr uint8_t MIN_ROVER_ID = 1;
        constexpr uint8_t MAX_ROVER_ID = 5;
    }
    
    // Color Scheme for Rovers (RGBA)
    namespace Colors {
        constexpr float ROVER_COLORS[5][4] = {
            {1.0f, 0.0f, 0.0f, 1.0f}, // Rover 1: Red
            {0.0f, 1.0f, 0.0f, 1.0f}, // Rover 2: Green
            {0.0f, 0.0f, 1.0f, 1.0f}, // Rover 3: Blue
            {1.0f, 1.0f, 0.0f, 1.0f}, // Rover 4: Yellow
            {1.0f, 0.0f, 1.0f, 1.0f}  // Rover 5: Magenta
        };
        
        constexpr float BACKGROUND_COLOR[4] = {0.1f, 0.1f, 0.1f, 1.0f}; // Dark gray
        constexpr float GRID_COLOR[4] = {0.3f, 0.3f, 0.3f, 0.5f};       // Light gray, semi-transparent
    }
    
    // Button Mapping (bit positions in VehicleTelem.buttonStates)
    namespace Buttons {
        constexpr uint8_t BUTTON_0 = 0x01; // Bit 0
        constexpr uint8_t BUTTON_1 = 0x02; // Bit 1
        constexpr uint8_t BUTTON_2 = 0x04; // Bit 2
        constexpr uint8_t BUTTON_3 = 0x08; // Bit 3
        constexpr uint8_t BUTTON_4 = 0x10; // Bit 4
        constexpr uint8_t BUTTON_5 = 0x20; // Bit 5
        constexpr uint8_t BUTTON_6 = 0x40; // Bit 6
        constexpr uint8_t BUTTON_7 = 0x80; // Bit 7
    }
}

// Utility macros for time conversion
#define MS_TO_NS(ms) ((ms) * 1000000ULL)
#define NS_TO_MS(ns) ((ns) / 1000000ULL)
#define SEC_TO_MS(sec) ((sec) * 1000ULL)
#define MS_TO_SEC(ms) ((ms) / 1000.0)

// Utility for getting current timestamp
namespace TimeUtils {
    inline double getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }
}