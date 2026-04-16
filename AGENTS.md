# SIMIA: Isometric Adventure Engine – AI Agent Guide

## Project Overview

SIMIA is a C++20 isometric game engine using **raylib** for rendering and **ImGui** for UI. It features a procedurally-generated terrain, animated character sprites, interactive camera controls, and real-time tuning via debug panels. The entire game loop, rendering, and logic is in a single `main.cpp` file.

## Build & Environment

**CMake-based (3.20+)** with C++20 standard. Raylib is auto-fetched via `FetchContent` if not on system.

```bash
# Typical build (from project root)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
./cmake-build-debug/SIMIA
```

**Key files:**
- `CMakeLists.txt` – Links raylib, includes ImGui sources and dependencies
- `dependencies/include/` – ImGui headers + raylib integration (rlImGui)

## Architecture Patterns

### 1. Single-File Game Loop (main.cpp)

All game logic, rendering, and state lives in `main.cpp` within an anonymous namespace.

**Key structures:**
- `GameTuning` – Live-editable float parameters (movement speed, camera angles, sprite scale)
- `AnimationClip` – Frame container (vector of Texture2D) + FPS metadata
- `WorldProp` – 3D world entities (trees/rocks with position, scale, type flag)
- `MoveState` enum – Idle/Walk/Run for animation state machine

**State variables in main():**
- Player position, velocity, rotation, stamina
- Animation time + frame index (advanced by frame time × FPS)
- Active camera (always follows player with lerp smoothing)

### 2. Rendering Layers (bottom to top)

1. **Terrain**: `DrawWorldTile()` generates height via perlin-like sine/cosine waves
   - Uses `TerrainHeight(x, z)` function with multi-frequency harmonics
   - Colors lerp between grass/dirt/water based on height and path proximity

2. **Props**: `DrawProp()` renders trees (cylinder trunk + sphere foliage) or rocks (single sphere)
   - Seeded randomization: `SetRandomSeed(1337)` ensures consistent prop placement per run
   - Props skip placement in water or near the center path

3. **Player**: Billboard sprite OR fallback cylinder character
   - Sprite uses `DrawBillboardRec()` with horizontal flip for direction
   - Frame source rectangle is mirrored when `faceLeft` is true
   - Fallback geometric player if animation frames missing

4. **UI**: ImGui windows (non-modal, fixed positions)
   - Adventure Slice panel: animation state + clip info
   - Tuning panel: real-time sliders for all GameTuning fields
   - Runtime panel: FPS, position, stamina meter

### 3. Animation System

**Fallible asset loading** via `ResolveExistingPath()` with three candidate directories per clip:
```cpp
LoadClip({"assets/NUDE_MONKEY/01-Idle/01-Idle", "assets/NUDE_MONKEY/Idle", "SIMIA/assets/..."},
         8.0f, "Idle");
```
Tries relative paths from current dir, parent, and grandparent—**essential for IDE/build system variance**.

**Frame selection:** `ChooseClip()` picks animation based on MoveState with graceful fallback:
- Run → Walk → Idle (if Run clip empty, use Walk, else Idle)
- Walk → Idle (if Walk empty, use Idle)

**Time advancement:** `animTime += dt * clip.fps` wraps at frame count. Simple modulo loop, no blending.

### 4. Physics & Movement

**Velocity-based**: Acceleration lerp smooths velocity toward desired direction.
```cpp
velocity = Vector3Lerp(velocity, desiredVel, Clamp(dt * tuning.acceleration, 0.0f, 1.0f));
```

**Camera-relative input**: Movement axes (WASD) rotate relative to camera yaw/pitch, not world axes:
```cpp
Vector3 camForward = {-std::cos(yaw), 0.0f, -std::sin(yaw)};
Vector3 moveWorld = Vector3Add(Vector3Scale(camForward, inputDir.z), 
                               Vector3Scale(camRight, inputDir.x));
```

**Stamina system**: Drains at `0.35 * dt` when sprinting, recharges at `0.2 * dt` otherwise.
Sprint requires stamina > 0.1.

**Terrain collision**: Player Y position pinned to `TerrainHeight(x, z) + 0.74f` every frame (instant, no sliding).
World bounds clamped to `±(kHalfTiles * kTileSize - 0.8f)`.

### 5. Camera System

**Fixed orbital camera (v2.1+)**:
```cpp
camera.target = Vector3Lerp(camera.target, cameraTarget, dt * 8.0f);
camera.position = Vector3Lerp(camera.position, offsetFromTarget, dt * 8.0f);
```

Camera angles are **locked at compile-time constants**:
- `cameraPitch = 30.0f` (degrees)
- `cameraDistance = 37.0f` (units)
- `cameraYaw = 45.0f` (degrees)

Camera target is offset 0.6 units above player. Offset vector computed from spherical coordinates
using fixed tuning parameters. No runtime adjustment available (removed from UI in v2.1).

---

## Development Workflow

### Add a New Game Parameter

1. Add field to `GameTuning` struct (line 26–37)
2. Add ImGui slider in Tuning panel (line 378–387)
3. Use `tuning.<field>` in game logic
4. **Example**: Adding `jumpForce` would be 3 lines of code

### Load New Animation Clips

1. Place PNG frames in `assets/NUDE_MONKEY/<AnimName>/` folder
2. Call `LoadClip({paths...}, fps, "ClipName")` in main() and assign to variable
3. Add `UnloadClip()` call before exit
4. Update `ChooseClip()` if adding new MoveState enum values

### Modify Terrain

- **Height function** (line 51–55): Adjust sine/cosine harmonics or add Perlin noise
- **Tile coloring** (line 72–80): Edit Color constants and lerp blend logic
- **Tile resolution**: Increase `kHalfTiles` or reduce `kTileSize`

### Extend World Props

Modify `BuildProps()` (line 92–113):
- Seed is fixed at 1337 for reproducibility
- Conditional placement via terrain height and path-proximity checks
- Easy to add new shape types: add struct field, branch in `DrawProp()`

### Configure NPC Behavior

1. Adjust `kNPCCount` for number of NPCs (default 10)
2. Modify movement speed in `UpdateNPC()` – change `Vector3Scale(moveDir, 3.0f)` to adjust NPC walking speed
3. Tweak animation selection: edit thresholds in `ChooseClip()` or NPC movement logic
4. Change spawn area by modifying `SpawnNPCs()` bounds (currently ±kHalfTiles ±3)
5. Adjust `directionChangeInterval` range (currently 2.0–5.0 seconds per NPC)

---

## NPC System (v2.0+)

**NPC Structure** contains:
- `position`, `velocity` – Current movement vectors
- `yaw` – Facing direction in radians
- `moveTimer`, `directionChangeInterval` – Time-based behavior scheduling
- `moveState` – Current animation state (Idle/Walk/Run)
- Animation: `activeClip`, `animTime`, `animFrameIndex`, `faceLeft`

**NPC Update Cycle** (called every frame for each NPC):
1. Increment `moveTimer`
2. Every `directionChangeInterval` seconds: randomly choose new direction or stay idle (30% idle, 70% walk)
3. Compute velocity based on movement state (3.0 units/sec when walking)
4. Apply velocity to position and clamp to world bounds
5. Pin Y position to terrain height
6. Update animation state and advance animation frame
7. Update facing direction based on velocity

**NPC Spawning**: `SpawnNPCs(kNPCCount)` creates NPCs with:
- Random position within safe bounds (avoid water, center path)
- Randomized `directionChangeInterval` (2.0–5.0 seconds)
- Seed 42 (different from terrain/props) for reproducible NPC placement

**NPC Rendering**: Each NPC drawn as billboard sprite with **alpha transparency (0.85f)**:
- Sprite height: 1.7f (smaller than player's 2.15f)
- Vertical offset: -0.07f for proper grounding
- **Alpha blending via `Fade(WHITE, 0.85f)`** ensures visibility even when camera passes behind
- Prevents billboard occlusion (classic raylib billboard issue where sprites disappear behind camera)

---

## Key Constants & Conventions

**Naming:**
- `k*` prefix for compile-time constants (e.g., `kScreenWidth`, `kTileSize`, `kNPCCount`)
- camelCase for local variables and parameters
- MoveState uses PascalCase (enum class values)

**Coordinates:**
- X, Z are horizontal; Y is vertical (raylib convention)
- `TerrainBaseY = -0.55f` is the "sea level" baseline
- World extends ±35 tiles (5x larger than v1.0; at 1.2 units per tile ≈ ±42 units)

**Timing:**
- Locked to 144 FPS target but delta-time aware (`dt = GetFrameTime()`)
- Animation FPS stored per clip; time accumulates and wraps

**Memory:**
- No dynamic allocation in game loop; props/clips built once at startup
- Textures loaded once, unloaded at shutdown; raylib handles GPU memory

---

## Cross-Component Communication

**No separate systems** — logic integrated into main loop:
1. Input → velocity update
2. Velocity → position, animation state
3. Position → camera target, terrain lookup
4. Animation state → clip selection
5. All renders in single frame: terrain → props → player → UI

**ImGui integration** via `rlImGui`:
- Window setup: `rlImGuiSetup()` at init, `rlImGuiShutdown()` at exit
- Per-frame: `rlImGuiBegin()` / `rlImGuiEnd()` wraps ImGui commands
- No separate input handling; ImGui consumes mouse/keyboard

---

## Common Pitfalls & Idioms

**Asset path resolution is fragile**: IDE runs with different working directories. Always use `ResolveExistingPath()` with multiple candidates. Build system may run from project root; IDE may run from `cmake-build-debug/`.

**Animation frame index wraps silently**: `animFrameIndex = animTime % frameCount`. No bounds check; relies on frame count > 0. Guard with `if (!clip->frames.empty())`.

**Sprite flipping via negative width**: `source.width = -width` mirrors the rectangle. RecalculateLayout when flipping direction.

**ImGui fixed windows**: `ImGui::SetNextWindowPos(..., ImGuiCond_Always)` locks position every frame. Change condition to `ImGuiCond_FirstUseEver` to allow user dragging.

**Lerp smoothing can feel sluggish**: Camera/velocity lerp use `dt * rate` clamped to [0, 1]. Increase rate constant (e.g., `dt * 12.0f`) for snappier feel.

---

## Testing & Debugging

**Built-in debug panels:**
- "Adventure Slice": Frame count, loaded clip count, warnings if clips empty
- "Tuning": Live-edit all GameTuning parameters (FPS-independent testing)
- "Runtime": FPS counter, player coordinates, props count, stamina meter

**Grid toggle**: `ImGui::Checkbox("Show Grid", &tuning.drawGrid)` overlays grid at 1.0 spacing for terrain verification.

**Prop rebuild**: Button in UI calls `BuildProps()` again, useful for randomization testing.

**Manual checks:**
- Verify `assets/NUDE_MONKEY/` subdirectories exist with .png files
- Check console for missing texture warnings (raylib logs unloaded assets)
- Adjust `cameraPitch`/`cameraYaw` sliders to verify isometric angle

---

## External Dependencies

- **raylib 5.0** (auto-fetched or system-installed): 3D graphics, windowing, math
- **ImGui** (bundled in `imgui/`): Immediate-mode UI
- **rlImGui** (`dependencies/include/rlImGui.h`): ImGui ↔ raylib integration
- **C++ stdlib**: `<filesystem>`, `<vector>`, `<algorithm>`, `<cmath>`

No external game frameworks; all game logic is C++ in `main.cpp`.

---

## Quick Modification Checklist

- [ ] **New uniform tuning parameter?** → Add to GameTuning, add slider
- [ ] **New animation state?** → Add MoveState enum value, update ChooseClip()
- [ ] **Terrain look change?** → Modify DrawWorldTile() colors or TerrainHeight() function
- [ ] **Camera feel off?** → Adjust tuning.cameraDistance, cameraPitch, cameraYaw, or lerp rate
- [ ] **Animation plays wrong?** → Check clip FPS in LoadClip(), verify frame count in ImGui Runtime panel
- [ ] **Assets missing warning?** → Run from project root or ensure assets/ path exists

