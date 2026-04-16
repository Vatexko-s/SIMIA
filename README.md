# SIMIA - Civilization Simulator

SIMIA is a C++20 isometric sandbox built on raylib + ImGui. It now runs a small civilization simulation with aging, death, reproduction, and genome inheritance.

## What Is Implemented

- Fixed-camera isometric world (pitch 30, distance 37, yaw 45)
- Procedural terrain + deterministic prop placement
- Player avatar with animated sprite and stamina movement
- Agent-based civilization simulation:
  - lifecycle: age, energy drain, fertility cooldown, death causes
  - reproduction: male/female pairing, offspring generation tracking
  - genetics: inherited traits with mutation
  - timeline stats: births/deaths per epoch, max generation, total population
- Hover + click interaction:
  - white border on hovered agent
  - `NPC Stats` inspector for selected agent

## Build

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
./cmake-build-debug/SIMIA
```

## Controls

- `W A S D` move player (camera-relative)
- `Left Shift` sprint
- Mouse hover over NPC -> white outline
- Left click NPC -> open stats panel
- `Cmd+Q` quit (macOS)

## Multi-file Architecture

- `main.cpp`
  - app bootstrap, camera, player movement, rendering orchestration, ImGui layout
- `src/AnimationSystem.h/.cpp`
  - clip loading/unloading, path resolution, animation clip selection
- `src/WorldSystem.h/.cpp`
  - terrain height/coloring, world tile rendering, prop generation/rendering
- `src/CivilizationSystem.h/.cpp`
  - agent population, fixed-step simulation, reproduction, mortality, timeline stats
- `src/SimulationTypes.h`
  - shared simulation types (`Agent`, `Genome`, `CivilizationStats`, enums)

## Simulation Model (Current)

Each agent has:

- identity: `id`, `generation`, `sex`
- lifecycle: `age`, `maxAge`, `energy`, `fertilityCooldown`, `alive`, `deathCause`
- genome: `moveSpeed`, `metabolism`, `fertility`, `longevity`
- motion/animation state: position, velocity, yaw, move state, animation frame

Fixed-step timeline:

- simulation tick: `0.1s`
- epoch length: `10s`
- runtime reports:
  - current living population
  - total born/dead
  - births/deaths this epoch
  - max generation reached
  - total simulated time

## Notes for Next Iteration

Recommended next steps (already compatible with current architecture):

1. Add resource nodes (food/water) so energy depends on gathering.
2. Add spatial partitioning for mate search (performance at high populations).
3. Add configurable balancing presets: stable / growth / collapse.
4. Persist timeline snapshots to CSV/JSON for offline GA analysis.
