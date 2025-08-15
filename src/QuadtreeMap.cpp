#include "QuadtreeMap.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

// ---- Utility time ----
double ElevationMap::nowSeconds() {
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now = clock::now();
    std::chrono::duration<double> d = now - start;
    return d.count();
}

// ---- Tile methods ----
static inline int childIndexFor(float x, float z, float cx, float cz) {
    // SW(0): x<cx,z<cz; SE(1): x>=cx,z<cz; NW(2): x<cx,z>=cz; NE(3): x>=cx,z>=cz
    int xi = (x >= cx) ? 1 : 0;
    int zi = (z >= cz) ? 1 : 0;
    // map (xi,zi) -> idx
    if (xi == 0 && zi == 0) return 0; // SW
    if (xi == 1 && zi == 0) return 1; // SE
    if (xi == 0 && zi == 1) return 2; // NW
    return 3; // NE
}

QuadNode* Tile::locateLeaf(float x, float z) {
    if (!root) root = std::make_unique<QuadNode>();
    QuadNode* node = root.get();
    float cx = originX + size * 0.5f;
    float cz = originZ + size * 0.5f;
    float half = size * 0.5f;
    for (int depth = 0; depth < maxDepth; ++depth) {
        if (node->isLeaf) {
            // Split if not at max depth
            if (depth == maxDepth - 1) {
                return node;
            }
            node->isLeaf = false;
            for (int i = 0; i < 4; ++i) {
                node->children[i] = std::make_unique<QuadNode>();
                // Initialize children from parent for continuity
                node->children[i]->isLeaf = true;
                node->children[i]->cell = node->cell;
            }
        }
        int idx = childIndexFor(x, z, cx, cz);
        // Update center for next level
        half *= 0.5f;
        cx += (idx == 1 || idx == 3) ? half : -half;
        cz += (idx >= 2) ? half : -half;
        node = node->children[idx].get();
    }
    return node;
}

static inline void emaUpdate(float& mean, float newVal, float alpha) {
    mean = mean + alpha * (newVal - mean);
}

void Tile::integratePoint(const LidarPoint& p, double nowTs,
                          float tauAccept, float tauReplace,
                          int K, int Nsat, int Nconf, float tauUpload) {
    QuadNode* leaf = locateLeaf(p.x, p.z);
    ElevCell& c = leaf->cell;
    if (!c.valid) {
        c.z_mean = p.y;
        c.prev_z_mean = c.z_mean;
        c.z_var = 0.0f;
        c.n = 1;
        c.disagreeHits = 0;
        c.flags |= (ELEV_VALID | ELEV_DIRTY | ELEV_CHANGED);
        c.valid = true;
        dirty = true;
        return;
    }
    float dz = std::fabs(p.y - c.z_mean);
    if (dz <= tauAccept) {
        // Running mean with saturation on n
        uint16_t nprime = static_cast<uint16_t>(std::min<int>(c.n + 1, Nsat));
        // Welford-like simple update
        float delta = p.y - c.z_mean;
        c.z_mean = c.z_mean + delta / std::max<uint16_t>(nprime, 1);
        // approximate variance blend
        c.z_var = 0.9f * c.z_var + 0.1f * (delta * delta);
        c.n = nprime;
        c.disagreeHits = 0;
        if (std::fabs(c.z_mean - c.prev_z_mean) > tauUpload) {
            c.prev_z_mean = c.z_mean;
            c.flags |= ELEV_DIRTY;
            dirty = true;
        }
    } else if (dz >= tauReplace) {
        // Within time window?
        if (nowTs - c.lastDisagreeTs <= 1.0) {
            if (c.disagreeHits < 255) c.disagreeHits++;
        } else {
            c.disagreeHits = 1;
        }
        c.lastDisagreeTs = nowTs;
        if (c.n < Nconf || c.disagreeHits >= K) {
            c.z_mean = p.y;
            c.prev_z_mean = c.z_mean;
            c.z_var = 0.0f;
            c.n = 1;
            c.disagreeHits = 0;
            c.flags |= (ELEV_CHANGED | ELEV_DIRTY | ELEV_VALID);
            c.valid = true;
            dirty = true;
        }
    } else {
        // gray zone: small-α EMA
        emaUpdate(c.z_mean, p.y, 0.1f);
        if (std::fabs(c.z_mean - c.prev_z_mean) > tauUpload) {
            c.prev_z_mean = c.z_mean;
            c.flags |= ELEV_DIRTY;
            dirty = true;
        }
        // decay disagreement if too old
        if (nowTs - c.lastDisagreeTs > 1.0) c.disagreeHits = 0;
    }
}

static float sampleLeafHeight(const QuadNode* node) {
    if (!node) return 0.0f;
    if (node->isLeaf) return node->cell.valid ? node->cell.z_mean : 0.0f;
    // average children
    float sum = 0.0f; int cnt = 0;
    for (int i = 0; i < 4; ++i) {
        if (node->children[i]) { sum += sampleLeafHeight(node->children[i].get()); cnt++; }
    }
    return (cnt > 0) ? (sum / cnt) : 0.0f;
}

void Tile::buildHeightGrid(int gridNVertices, std::vector<float>& outHeights) const {
    outHeights.resize(static_cast<size_t>(gridNVertices) * static_cast<size_t>(gridNVertices));
    if (!root) {
        std::fill(outHeights.begin(), outHeights.end(), 0.0f);
        return;
    }
    float step = size / static_cast<float>(gridNVertices - 1);
    for (int j = 0; j < gridNVertices; ++j) {
        for (int i = 0; i < gridNVertices; ++i) {
            float x = originX + i * step;
            float z = originZ + j * step;
            // Traverse to leaf
            const QuadNode* node = root.get();
            float cx = originX + size * 0.5f;
            float cz = originZ + size * 0.5f;
            float half = size * 0.5f;
            for (int depth = 0; depth < maxDepth - 1 && node && !node->isLeaf; ++depth) {
                int idx = childIndexFor(x, z, cx, cz);
                half *= 0.5f;
                cx += (idx == 1 || idx == 3) ? half : -half;
                cz += (idx >= 2) ? half : -half;
                node = node->children[idx].get();
            }
            float y = node ? (node->isLeaf ? (node->cell.valid ? node->cell.z_mean : 0.0f) : sampleLeafHeight(node)) : 0.0f;
            outHeights[j * gridNVertices + i] = y;
        }
    }
}

// ---- ElevationMap ----
ElevationMap::ElevationMap() {
    setParameters(32.0f, 0.25f, 0.25f, 0.7f, 3, 20, 5, 0.06f, 1.0f);
}

void ElevationMap::setParameters(float tileSizeMeters,
                                 float baseCellResolutionMeters,
                                 float tauAcceptMeters,
                                 float tauReplaceMeters,
                                 int K_confirm,
                                 int Nsat_cap,
                                 int Nconf_low,
                                 float tauUploadMeters,
                                 float deltaT_windowSeconds) {
    tileSize = tileSizeMeters;
    baseCellRes = baseCellResolutionMeters;
    tauAccept = tauAcceptMeters;
    tauReplace = tauReplaceMeters;
    K = K_confirm;
    Nsat = Nsat_cap;
    Nconf = Nconf_low;
    tauUpload = tauUploadMeters;
    disagreeWindow = deltaT_windowSeconds;

    // Derive maxDepth and grid vertex count
    float cellsPerTile = tileSize / baseCellRes;
    int power = 0;
    int c = 1;
    while (c < static_cast<int>(std::round(cellsPerTile)) && power < 10) { c <<= 1; power++; }
    maxDepth = power; // depth produces 2^power cells along edge
    int gridN = (1 << power) + 1;
    gridNVertices = gridN;
}

Tile& ElevationMap::getOrCreateTile(int tx, int tz) {
    TileKey key{tx, tz};
    auto it = tiles.find(key);
    if (it != tiles.end()) return it->second;
    float ox = tx * tileSize;
    float oz = tz * tileSize;
    Tile t(ox, oz, tileSize, maxDepth);
    auto [insIt, _] = tiles.emplace(key, std::move(t));
    return insIt->second;
}

void ElevationMap::integrateScan(const std::vector<LidarPoint>& points, double nowTs) {
    for (const auto& p : points) {
        int tx = static_cast<int>(std::floor(p.x / tileSize));
        int tz = static_cast<int>(std::floor(p.z / tileSize));
        Tile& tile = getOrCreateTile(tx, tz);
        tile.integratePoint(p, nowTs, tauAccept, tauReplace, K, Nsat, Nconf, tauUpload);
    }
}

std::vector<TileUpdate> ElevationMap::consumeDirtyTiles() {
    std::vector<TileUpdate> updates;
    for (auto& kv : tiles) {
        TileKey key = kv.first;
        Tile& t = kv.second;
        if (!t.dirty) continue;
        TileUpdate up;
        up.key = key;
        up.tileSize = tileSize;
        t.buildHeightGrid(gridNVertices, up.heights);
        updates.push_back(std::move(up));
        t.dirty = false;
    }
    return updates;
}

ElevationStats ElevationMap::getStats() const {
    ElevationStats st{};
    st.numTiles = tiles.size();
    size_t leaves = 0;
    // rough count via DFS
    std::vector<const QuadNode*> stack;
    for (const auto& kv : tiles) {
        if (kv.second.root) stack.push_back(kv.second.root.get());
        while (!stack.empty()) {
            const QuadNode* n = stack.back(); stack.pop_back();
            if (n->isLeaf) {
                leaves++;
            } else {
                for (int i = 0; i < 4; ++i) if (n->children[i]) stack.push_back(n->children[i].get());
            }
        }
    }
    st.numLeaves = leaves;
    return st;
}

std::vector<TileUpdate> ElevationMap::consumeDirtyTilesBudgeted(size_t maxBytes) {
    size_t perTile = static_cast<size_t>(gridNVertices) * static_cast<size_t>(gridNVertices) * sizeof(float);
    if (perTile == 0) return {};
    size_t budgetTiles = maxBytes / perTile;
    if (budgetTiles == 0) budgetTiles = 1;
    std::vector<TileUpdate> updates;
    updates.reserve(budgetTiles);
    for (auto& kv : tiles) {
        if (updates.size() >= budgetTiles) break;
        TileKey key = kv.first;
        Tile& t = kv.second;
        if (!t.dirty) continue;
        TileUpdate up;
        up.key = key; up.tileSize = tileSize;
        t.buildHeightGrid(gridNVertices, up.heights);
        updates.push_back(std::move(up));
        t.dirty = false;
    }
    return updates;
}


