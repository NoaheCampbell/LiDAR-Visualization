#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "NetworkTypes.h"

struct ElevCell {
    float z_mean = 0.0f;
    float z_var = 0.0f;
    uint16_t n = 0;
    uint8_t disagreeHits = 0;
    uint8_t age = 0;
    uint8_t flags = 0; // bitfield: 1=STABLE, 2=CHANGED, 4=DIRTY, 8=VALID
    float prev_z_mean = 0.0f;
    double lastDisagreeTs = 0.0;
    bool valid = false;
};

enum ElevFlags : uint8_t {
    ELEV_STABLE  = 1u << 0,
    ELEV_CHANGED = 1u << 1,
    ELEV_DIRTY   = 1u << 2,
    ELEV_VALID   = 1u << 3,
};

struct TileKey {
    int tx = 0; // tile index in X
    int tz = 0; // tile index in Z
    bool operator<(const TileKey& o) const {
        if (tx != o.tx) return tx < o.tx;
        return tz < o.tz;
    }
};

// Simple quadtree with fixed maximum depth. Children index order: (0: SW, 1: SE, 2: NW, 3: NE)
struct QuadNode {
    bool isLeaf = true;
    ElevCell cell;
    std::unique_ptr<QuadNode> children[4];
};

struct Tile {
    // World-space origin (min corner) and size (square)
    float originX = 0.0f;
    float originZ = 0.0f;
    float size = 32.0f;
    int maxDepth = 7; // 2^7 = 128 -> 0.25 m cells for 32 m tiles
    bool dirty = false; // mark when any cell meaningfully changes

    std::unique_ptr<QuadNode> root;

    Tile() = default;
    Tile(float ox, float oz, float s, int depth) : originX(ox), originZ(oz), size(s), maxDepth(depth) {
        root = std::make_unique<QuadNode>();
    }

    QuadNode* locateLeaf(float x, float z);
    void integratePoint(const LidarPoint& p, double nowTs,
                        float tauAccept, float tauReplace,
                        int K, int Nsat, int Nconf, float tauUpload);

    // Builds a dense (N+1)x(N+1) height grid covering the tile by sampling leaf z_mean.
    void buildHeightGrid(int gridNVertices, std::vector<float>& outHeights) const;
};

struct TileUpdate {
    TileKey key;
    std::vector<float> heights; // (N+1)^2 heights row-major (z-major rows), Y-up
    float tileSize = 32.0f;
};

struct ElevationStats {
    size_t numTiles = 0;
    size_t numLeaves = 0;
};

class ElevationMap {
public:
    ElevationMap();

    void setParameters(float tileSizeMeters,
                       float baseCellResolutionMeters,
                       float tauAcceptMeters,
                       float tauReplaceMeters,
                       int K_confirm,
                       int Nsat_cap,
                       int Nconf_low,
                       float tauUploadMeters,
                       float deltaT_windowSeconds);

    void integrateScan(const std::vector<LidarPoint>& points, double nowTs);

    // Returns dirty tiles with fully rebuilt height grids and clears their dirty flags.
    std::vector<TileUpdate> consumeDirtyTiles();
    // Budgeted variant: limit total approximate bytes uploaded this frame.
    std::vector<TileUpdate> consumeDirtyTilesBudgeted(size_t maxBytes);

    ElevationStats getStats() const;

    int getGridNVertices() const { return gridNVertices; }
    float getTileSize() const { return tileSize; }

private:
    float tileSize = 32.0f;
    float baseCellRes = 0.25f;
    int maxDepth = 7; // derived from tileSize / baseCellRes

    float tauAccept = 0.25f;
    float tauReplace = 0.7f;
    int K = 3;
    int Nsat = 20;
    int Nconf = 5;
    float tauUpload = 0.06f;
    float disagreeWindow = 1.0f; // seconds

    int gridNVertices = 129; // default for 0.25m cells over 32m tile

    std::map<TileKey, Tile> tiles;

    static double nowSeconds();
    Tile& getOrCreateTile(int tx, int tz);
};


