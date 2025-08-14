# Rover Emulator - Overview

The rover emulator simulates multiple autonomous rovers by reading telemetry data from predefined files and streaming it over UDP on localhost. The emulator reads position and LiDAR data at a fixed 10Hz frequency.

For debugging purposes, a commandline argument can be specified to elminate simulated sensor noise in the readings. The solution will be evaluated off of its ability to handle the program running without the --no-noise flag, however. See `Running the Emulator` for details

## Directory Structure
```
/project_root
│── emulator/
│   ├── rover_emulator.cpp  # Main source file
│   ├── rover_profiles.h    # Rover profile definitions
│── data/                   # Contains rover data files
│── run_rovers.sh           # Script to launch multiple rovers
│── Makefile                # Compilation instructions
```

## Compilation Instructions
To compile the emulator, run:
```sh
make
```

To clean up compiled files:
```sh
make clean
```

Data files required for the emulator are .gz compressed inside data/
run `make extract` to extract them.

## Running the Emulator
To run the full set of 5-rover emulators with full noise characteristics, run
```sh
make run
```
To run without noise for debugging purposes, run
```sh
make run-noiseless
```

To manually start an emulator for a specific rover with an optional `--no-noise` flag
```sh
./rover_emulator <ROVER_ID>
```
Example:
```sh
./rover_emulator 1
```

To run five concurrent rover instances, use:
```sh
./run_rovers.sh
```


## Termination
If `run_rovers.sh` is terminated, all running rover instances are killed automatically.

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
