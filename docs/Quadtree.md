# Quadtree Elevation Map: Agree/Disagree Mapping for Stable FPS
**Version:** 1.0  
**Scope:** Terrain-first, resource-light map integration and rendering for the multi-rover LiDAR viewer.  
**Context:** Integrates with the provided rover emulator (UDP pose/LiDAR/telemetry at 10 Hz, chunked LiDAR packets, per-rover port mapping):contentReference[oaicite:0]{index=0}:contentReference[oaicite:1]{index=1}:contentReference[oaicite:2]{index=2}.

---

## 1) Purpose & Rationale
We need a persistent world that **doesn’t get slower as more points arrive**. Instead of a heavy 3D voxel grid, we use a **2.5D elevation map** backed by **adaptive quadtrees**:

- **Agree vs. disagree logic** determines whether to **integrate** new samples or **remap** changed areas.
- **Adaptive detail** (quadtree split/merge) gives high resolution where terrain is complex; coarse cells where flat.
- **GPU uploads are bounded**: we only push **dirty tiles/patches** when cell statistics change enough.

This fits construction-site use: lots of ground, evolving piles/grades, and machinery, all streamed at **10 Hz** from five rovers over UDP with chunked LiDAR packets and fixed packet structs:contentReference[oaicite:3]{index=3}.

---

## 2) Data Model

### 2.1 World Tiling
- World is partitioned into square **tiles** (e.g., 32 m × 32 m in world meters).
- Each tile owns a **quadtree** over (x,y). The **leaf** represents a rectangular cell of ground.

### 2.2 Leaf Cell Statistics
Minimal storage (no point arrays):
```cpp
struct ElevCell {
  float  z_mean;         // running mean elevation
  float  z_var;          // elevation variance
  uint16_t n;            // sample count (clamped to Nsat)
  uint8_t disagreeHits;  // consecutive disagreements in Δt
  uint8_t age;           // for fading/eviction policies
  uint8_t flags;         // bitfield: STABLE, CHANGED, DIRTY, VALID
};
```
- **One sample “slot” per cell** → memory & render cost are bounded by #leaves.

### 2.3 Optional Sparse Overlay (non-ground)
To retain simple vertical detail (e.g., vehicles, materials):
- Keep a **small capped set** of “overlay points” per tile (e.g., ≤2–8 k), clustered/decimated.
- Render as **instanced billboards/surfels**; this layer is separate from the elevation map.

---

## 3) Input Integration (Emulator)

- **LiDAR input:** Arrives as **chunked scans**; collect by `timestamp`, assembling when all `totalChunks` are seen or after a timeout (e.g., 200 ms):contentReference[oaicite:4]{index=4}.
- **Pose:** Use the rover pose to transform LiDAR into the **world frame** if needed (depends on how `.dat` was authored):contentReference[oaicite:5]{index=5}.
- **Ports:** For rover *i*, Pose `9000+i`, LiDAR `10000+i`, Telemetry `11000+i`, Commands `8000+i`:contentReference[oaicite:6]{index=6}:contentReference[oaicite:7]{index=7}.
- **Rate:** 10 Hz for pose/LiDAR/telemetry:contentReference[oaicite:8]{index=8}.

---

## 4) Update Logic (Agree vs. Disagree)

For each point **p=(x,y,z)** in an assembled scan:

1. **Locate tile** and **quadtree leaf** that contains (x,y). If leaf doesn’t exist, create it at the current adaptive depth (see §5).

2. **Compute elevation delta:**  
   `dz = |z - z_mean|` (if cell is new/invalid, treat as large).

3. **Agree (integrate):** If `dz ≤ τ_accept`  
   - Update running stats (bounded growth):
     ```cpp
     n'     = min<uint16_t>(n+1, Nsat);
     z_mean = z_mean + (z - z_mean) / n';          // or EMA for constant time
     z_var  ≈ blend(z_var, (z - z_mean)*(z - z_mean));
     ```
   - Set `disagreeHits = 0`.
   - Only mark **DIRTY** if `|z_mean - z_mean_prev| > τ_upload` (prevents constant GPU churn).

4. **Disagree (candidate change):** If `dz ≥ τ_replace`  
   - Increment `disagreeHits` (with a short **time window** Δt; otherwise reset).
   - If **confidence low** (`n < Nconf`) **or** `disagreeHits ≥ K`, then **remap**:  
     ```
     z_mean = z; z_var = σ0²; n=1; disagreeHits=0; flags |= CHANGED|DIRTY;
     ```

5. **Between thresholds (gray zone):**  
   - Do a **soft update** (small-α EMA) without marking DIRTY, unless the mean moves > τ_upload.

**Interpretation:**  
- Areas with consistent measurements get integrated (no geometry explosion).  
- Only **persistently different** evidence flips the local elevation (**remap**).  
- GPU is updated **only when it matters** (DIRTY + Δz above threshold).

---

## 5) Quadtree Adaptivity

### 5.1 Split Criteria (increase resolution)
- Split a leaf if any of the following exceed thresholds within the leaf’s footprint:
  - **Elevation variance** `z_var > σ_split²` (terrain roughness).
  - **Local slope** from neighbor cells exceeds `slope_split` (edges, berms).
  - **Point density** in the leaf exceeds `ρ_split` (many returns → finer detail justified).

### 5.2 Merge Criteria (reduce resolution)
- Consider merging four sibling leaves if **all**:
  - Variance below `σ_merge²` and slopes below `slope_merge`.
  - No recent changes (`CHANGED` unset for T seconds).
  - Similar means (pairwise |Δz| < `z_merge_epsilon`).

**Note:** On split, initialize children by **interpolating** parent mean; on merge, compute parent mean as weighted average of children.

---

## 6) Rendering Pipeline

### 6.1 GPU Geometry
- Each **tile** owns a static **grid mesh** (e.g., `(N+1)×(N+1)` vertices for an `N×N` cell layout at that tile’s *current max* quadtree depth).
- The **elevation per grid vertex** is updated from the **current leaf means** of underlying cells.
- Implementation choices:
  - **Vertex texture / SSBO** holding per-vertex `z_mean` (preferred).
  - Or dynamic VBO sub-updates.

### 6.2 Dirty Region Updates
- Maintain a **dirty mask** per tile (or per small patch, e.g., 16×16 vertices).
- When cells flip to DIRTY, map them to their **covered vertices** and set those patches DIRTY.
- **Upload budget** per frame (e.g., ≤ 5–10 MB); queue excess patches to the next frames → consistent FPS.

### 6.3 Shading & Coloring
- Base color by elevation or slope.
- Optional **provenance tint** by rover ID (8-bit mask per cell tracks recent contributors).
- **Sparse overlay** (non-ground clusters) rendered as instanced quads/points with camera-facing billboards.

### 6.4 Culling & LOD
- **Frustum culling** per tile.
- Distance-based **vertex stride** or tessellation level.
- Optionally **skip** tiles beyond a max distance (minimap can still show them).

---

## 7) Performance & Resource Targets

| Component                     | Target            |
|------------------------------|-------------------|
| Network decode + scan build  | ≤ 5 ms / frame    |
| Map integration (CPU)        | ≤ 5–8 ms / frame  |
| GPU patch uploads            | ≤ 10 MB / frame   |
| Rendering (60 FPS target)    | ≤ 16 ms / frame   |
| End-to-end latency           | ≤ 50 ms           |

**Bounded growth:** cell count bounded by tiles × max leaves; **no per-point growth** in GPU buffers.

---

## 8) Parameters (Initial Defaults)

- **Tile size:** 32 m × 32 m  
- **Base cell resolution:** 0.25 m (quadtree may split to 0.125 m in complex areas)  
- **τ_accept:** 0.20–0.30 m  
- **τ_replace:** 0.60–0.80 m  
- **K (confirm change):** 3 hits within **Δt=1 s**  
- **Nsat (confidence cap):** 20 samples  
- **Nconf (low-confidence):** 5 samples  
- **τ_upload (GPU update):** 0.05–0.08 m  
- **σ_split² / σ_merge²:** tune empirically (start 0.04 / 0.01 m²)  
- **slope_split / slope_merge:** 0.25 / 0.10 (Δz/Δx)  
- **Eviction/fade:** decrement `age` each second; de-prioritize old, unchanged tiles for uploads.

---

## 9) Handling Non-Ground & Dynamics

- **Ground gate:** If `|z − z_mean| ≤ ζ_ground` (e.g., 0.25 m) → treat as ground; else candidate for **overlay**.
- **Overlay clustering:** Simple radius or grid clustering per tile, **cap total** overlay instances, and re-sample with reservoir or RPS when over budget.
- **Dynamic objects:** High disagreement over time at the same (x,y) with alternating elevations → keep in overlay; **do not remap ground** unless sustained.

---

## 10) Error Handling & Robustness

- **LiDAR chunk timeouts:** If not all chunks for a timestamp arrive within 200 ms, process partial or drop the scan (configurable):contentReference[oaicite:9]{index=9}.
- **Rover dropout:** If no pose for >1 s, label rover offline; continue rendering map; auto-resume on next packet.
- **Command loop (buttons):** UI toggles send 1-byte bitmask to `8000+i`; telemetry echoes state at 10 Hz for verification:contentReference[oaicite:10]{index=10}.

---

## 11) Pseudocode (Integration Loop)

```cpp
// Each frame or when a full scan is assembled:
for (const Vec3& p : scan.points_world) {
  Tile& T = tiles[tileIndex(p.x, p.y)];
  QuadLeaf* L = T.locateLeaf(p.x, p.y);            // adaptive lookup

  if (!L->valid) {
    initCell(*L, p.z); L->flags |= DIRTY; continue;
  }

  float dz = fabsf(p.z - L->z_mean);
  if (dz <= tau_accept) {
    integrate(L->z_mean, L->z_var, L->n, p.z, Nsat);
    if (fabsf(L->z_mean - L->prev_z_mean) > tau_upload) {
      markDirtyPatch(T, *L); L->prev_z_mean = L->z_mean;
    }
    L->disagreeHits = 0;
  } else if (dz >= tau_replace) {
    if (withinDeltaT(L->lastDisagreeTs, now)) L->disagreeHits++; else L->disagreeHits = 1;
    L->lastDisagreeTs = now;

    if (L->n < Nconf || L->disagreeHits >= K) {
      remap(L, p.z); L->disagreeHits = 0; markDirtyPatch(T, *L);
    }
  } else {
    // gray zone: small-α EMA, typically no GPU update unless Δz exceeds tau_upload
    emaUpdate(L->z_mean, p.z, 0.1f);
    if (fabsf(L->z_mean - L->prev_z_mean) > tau_upload) {
      markDirtyPatch(T, *L); L->prev_z_mean = L->z_mean;
    }
  }

  // Optional: split/merge checks on a budget (not every point)
  maybeSplitOrMerge(T, *L);
}

// End of frame:
uploadDirtyPatchesWithinBudget();   // e.g., ≤10 MB/frame
renderTiles();                      // heightmap
renderOverlay();                    // sparse non-ground
```

---

## 12) Testing Checklist

- **Functional**
  - Start emulator (`make run` / `./run_rovers.sh`) and verify map builds and stabilizes:contentReference[oaicite:11]{index=11}.
  - Toggle rover buttons and confirm telemetry round-trip.
- **Performance**
  - Tight scene sweep: ensure FPS ≥ 60 with bounded GPU uploads.
  - Dense scans: verify quadtree splits only where needed; uploads capped per frame.
- **Robustness**
  - Drop chunks artificially; confirm timeouts don’t stall the pipeline.
  - Kill and restart one rover; verify offline/online behavior without app restart.

---

## 13) Why This Meets Requirements
- **Real-time & low latency:** bounded CPU (constant-time stats per point) and bounded GPU traffic (dirty-patch uploads).
- **Persistent terrain:** elevation map stores where rovers have covered; updated incrementally.
- **Change awareness:** sustained disagreement triggers **remap**; small noise is ignored.
- **Lightweight:** 2.5D statistics instead of 3D voxels; adaptive detail only where needed.
- **Clean integration:** aligns with emulator packet formats, ports, and 10 Hz cadence:contentReference[oaicite:12]{index=12}:contentReference[oaicite:13]{index=13}:contentReference[oaicite:14]{index=14}.
