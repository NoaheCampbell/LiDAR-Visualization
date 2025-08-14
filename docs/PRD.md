# **Project Requirements Document (Enhanced)**  
**Title:** Multi-Rover LiDAR Visualization & Control Interface  
**Version:** 1.2  

---

## **1. Overview**
We are building a **real-time desktop GUI** that:
1. Connects to **five simulated autonomous rovers** over UDP (localhost) using fixed ports defined in `rover_profiles.h`.
2. **Visualizes** rover movement and LiDAR point cloud data in a **3D scene**.
3. Provides a **control interface** to send rover button commands (bits 0–3).
4. Updates **terrain maps** persistently as LiDAR data streams in.
5. Runs on **Linux** (tested on Ubuntu 22.04) with **<50 ms** end-to-end latency.

The rovers are emulated via the provided **`rover_emulator`** program, which streams:
- **Pose packets** (position & orientation)
- **LiDAR packets** (chunked point clouds)
- **Button telemetry** (states of buttons 0–3)  
and listens for **button command** packets.

---

## **2. Goals**
- **Performance:** Handle all 5 rovers at 10 Hz each without visible lag.
- **Usability:** Easy to switch between rover views and issue commands.
- **Clarity:** Intuitive 3D visualization of terrain & rover movement.
- **Maintainability:** Modular networking, rendering, and UI code.
- **Reliability:** Gracefully handle missing data, packet loss, and emulator restarts.

---

## **3. Network Port Mapping**

| Rover ID | Pose Port | LiDAR Port | Telemetry Port | Command Port |
|----------|-----------|------------|----------------|--------------|
| 1        | 9001      | 10001      | 11001          | 8001         |
| 2        | 9002      | 10002      | 11002          | 8002         |
| 3        | 9003      | 10003      | 11003          | 8003         |
| 4        | 9004      | 10004      | 11004          | 8004         |
| 5        | 9005      | 10005      | 11005          | 8005         |

All traffic is **UDP** on `127.0.0.1`.  
Commands are **1 byte** where bits 0–3 represent button states.

---

## **4. Inputs / Outputs**

### **4.1 Data Sources**
**Pose (`PosePacket`):**
struct PosePacket {
    double timestamp;  
    float posX, posY, posZ;
    float rotXdeg, rotYdeg, rotZdeg;
};

**LiDAR (`LidarPacket` chunks):**
struct LidarPacketHeader {
    double timestamp;
    uint32_t chunkIndex, totalChunks, pointsInThisChunk;
};
struct LidarPoint { float x, y, z; };

- Max 100 points per packet, multiple chunks per scan.
- All chunks for the same scan share a timestamp.

**Button Telemetry (`VehicleTelem`):**
struct VehicleTelem {
    double timestamp;
    uint8_t buttonStates; // bits 0–3
};

**Button Commands:**
- 1-byte UDP message → `cmdPort`.
- Bit 0 = Button 0, Bit 1 = Button 1, etc.

---

## **5. Functional Requirements**

### **5.1 Visualization**
- **3D Scene**
  - Flat starting plane.
  - Low-poly rover models with unique colors.
  - Camera:  
    - **Free-fly mode** (WASD + mouse look, speed adjustable).  
    - **Follow mode** (locks to selected rover, with smooth interpolation).
- **LiDAR Terrain**
  - Maintain **persistent point cloud per rover** in memory.
  - Merge into a **global terrain buffer** for rendering.
  - Store points with `(x, y, z, roverID, timestamp)`.
  - **Persistence policy:** Keep last **N million points** (configurable) to manage memory; discard oldest when limit reached.
  - Optional **decimation** (keep 1 in N points) and **fade** (drop points older than configurable time window).
- **Pose**
  - Update rover transforms from `PosePacket` in real time.

---

### **5.2 UI Panels**
- **Rover Selector**
  - Buttons or dropdown for rovers 1–5 (dynamic list from config).
- **Telemetry Panel**
  - XYZ position, XYZ rotation.
  - Button state indicators (4 toggles with ON/OFF).
- **Command Controls**
  - Toggle buttons update local bitmask and send to `cmdPort`.
- **Diagnostics Panel**
  - Last packet timestamp per stream type (pose, LiDAR, telemetry).
  - Packet loss rate = `(missingChunks / expectedChunks) * 100%` over rolling 5 s window.
  - Latency (pose timestamp to render time).
  - FPS, point count.
- **Mini-map** (optional)
  - Top-down rover positions.

---

### **5.3 LiDAR Assembly Logic**
- Maintain a per-rover buffer keyed by `(timestamp)`.
- Append each incoming chunk to the matching timestamp set.
- If all chunks (`chunkIndex` 0..`totalChunks-1`) received → merge into terrain buffer and free temp storage.
- If not all chunks arrive within **200 ms** of first chunk → discard partial scan and log packet loss.

---

### **5.4 UI Interaction Flow**
Example: **Toggle Button 2 for Rover 3**
1. User clicks “Button 2” in Rover 3’s control panel.
2. UI updates bitmask (`0b00000100`).
3. Sends to port `8003` via UDP.
4. Rover updates state, sends telemetry in next 10 Hz cycle.
5. UI reflects updated button state from telemetry.

---

## **6. Technical Design**

### **6.1 Architecture**
Modules:
1. **NetworkManager**
   - UDP socket handling (non-blocking).
   - Separate inbound threads for pose, LiDAR, telemetry per rover.
   - Outbound for button commands.
2. **DataAssembler**
   - LiDAR chunk reassembly.
   - Terrain persistence policy.
3. **Renderer**
   - OpenGL or Vulkan.
   - Draw rovers & point clouds (color by rover).
4. **UI Layer**
   - ImGui-based interface.
   - Panels for selection, control, diagnostics.
5. **Config Loader**
   - Reads rover list from `rover_profiles.h` or JSON config.

---

### **6.2 Performance Targets**
| Component                  | Target Time |
|----------------------------|-------------|
| Network polling & decode   | ≤ 5 ms      |
| LiDAR chunk assembly       | ≤ 5 ms      |
| Rendering                  | ≤ 16 ms     |
| UI update                  | ≤ 4 ms      |
| Total frame budget         | ≤ 33 ms     |
| End-to-end latency         | ≤ 50 ms     |

---

### **6.3 Error Handling**
- **Missing Packets:** Discard incomplete LiDAR scans after 200 ms.
- **Rover Disconnection:** If no `PosePacket` for >1 s → mark rover “offline”.
- **Reconnection:** Resume automatically on new data.
- **Command Send Failures:** Retry once after 50 ms.

---

## **7. Development Plan**
1. **Networking Layer**  
   - Implement UDP receive/send.
   - Map ports from rover profiles.
2. **Data Processing**  
   - Parse `PosePacket`, `LidarPacket`, `VehicleTelem`.
   - Implement chunk reassembly.
3. **Rendering**  
   - Draw rover models.
   - Render LiDAR points in real time.
4. **UI**  
   - Build rover selector, telemetry, control panel.
   - Add diagnostics.
5. **Persistence**  
   - Implement memory limit & fade/decimation.
6. **Polish**  
   - Camera controls.
   - Configurable settings.
   - Latency profiling.

---

## **8. Testing**
- **Functional**
  - Verify positions match emulator.
  - Toggle buttons → verify telemetry matches.
- **Performance**
  - Measure packet-to-render time with 5 rovers at 10 Hz.
- **Error Handling**
  - Simulate dropped LiDAR chunks (modify emulator).
  - Kill/restart emulator mid-run.
- **Persistence**
  - Verify old points fade/decimate as configured.

---

## **9. Deliverables**
- Source code with Linux build instructions.
- Executable connecting to provided emulator.
- README including:
  - Port mapping.
  - Controls.
  - Dependencies.
  - Configurable parameters.
