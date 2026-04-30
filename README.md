# SIMIA - Civilization Simulator

SIMIA is a C++20 isometric sandbox built on raylib + ImGui. It runs a real-time monkey civilization simulation with foraging, hydration, aging, reproduction, and inheritable genetics. The player walks the world and inspects individual monkeys via proximity interaction.

## Features

- **World**: Randomized isometric terrain (time-seeded each launch), tree-heavy prop scatter, larger 121x121 tile map, water lakes, fixed orbital camera.
- **Resources**:
  - Berry bushes (small, fast regrow) and fruit trees (large, slow regrow) restore energy.
  - Water spots restore hydration; rendered as cyan ripple discs over lakes.
- **Agents (monkeys)** have:
  - Lifecycle: age, energy, hydration, fertility cooldown, alive/dead, cause of death.
  - Genome: `moveSpeed`, `metabolism`, `fertility`, `longevity`, `vision`, `strength` (inherited with mutation).
  - Behavior FSM: `Wander`, `SeekFood`, `Eat`, `SeekWater`, `Drink`, `SeekMate`, `Rest`. Healthy fertile agents actively pursue partners; hungry/thirsty agents prioritize survival.
  - Sex-based reproduction: matures around year 8, mates with nearby fertile partner, energy/hydration gated.
- **Player**:
  - WASD walking with infinite sprint (no stamina drain).
  - Proximity interaction ring around the player.
- **Inspector** (press `E`):
  - Vital signs: energy / hydration / age bars + behavior + position + reproduction stats.
  - Toggle a dedicated **DNA panel** with a 3D auto-rotating double-helix (rendered to texture) and per-gene expression bars + composite fitness index.
- **Timeline + Stats**:
  - Year counter with progress bar, pause/step controls, world respawn.
  - Live tally: population, fertile males/females, foraging/eating/drinking counts, deaths by age/starvation, max generation, etc.

## Build

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
./cmake-build-debug/SIMIA
```

raylib 5.0 is auto-fetched if not installed.

## Controls

| Key            | Action                                      |
|----------------|---------------------------------------------|
| `W A S D`      | Walk (camera-relative)                      |
| `Left Shift`   | Sprint (unlimited)                          |
| `E`            | Inspect nearest monkey within 2.4 units     |
| `E` again      | Close inspector                             |
| `Esc`          | Close inspector / DNA panel                 |
| `Cmd + Q`      | Quit (macOS)                                |

A blue ring on a monkey + a `Press [E] to inspect` prompt above the head appear when you are in range. Selected monkey is tinted yellow with a gold ring. The mouse is no longer used for selection.

## Architecture

- `main.cpp` - bootstrap, camera, player movement, render orchestration, ImGui layout, DNA helix renderer.
- `src/AnimationSystem.{h,cpp}` - clip loading, path resolution, clip selection.
- `src/WorldSystem.{h,cpp}` - terrain, props, **food/water nodes** (build, regrow, draw).
- `src/CivilizationSystem.{h,cpp}` - agent population, fixed-step simulation, foraging FSM, reproduction, mortality, stats, agent lookup helpers.
- `src/SimulationTypes.h` - shared types (`Agent`, `Genome`, `Behavior`, `CivilizationStats`, enums).

## Simulation Model

Each agent runs a continuous behavior loop and a fixed 0.1 s aging step.

- Energy drains slowly with metabolism + activity + population pressure (tuned for long survival); refilled by eating berries / fruit.
- Hydration drains independently at the same slow rate; refilled at water nodes.
- Behavior priority: critical thirst > critical hunger > seek mate (when energy >= 0.65 and hydration >= 0.55) > top-up food/water > wander.
- Fertility window: age 8 - 84 % of `maxAge`, requires energy > 0.45 and hydration > 0.35.
- Children inherit the average of parents' genes with a 10 % chance of small mutation, clamped to `[0.5, 1.6]`.
- Death by old age (`age > maxAge`) or starvation (`energy <= 0` or `hydration <= 0`).
- 1 simulated year = 3 real seconds.

## DNA Visualization

The DNA panel shows:

1. A 3D **double-helix** rendered to its own RenderTexture with an orbiting camera (auto-spins each frame). Two cylinder backbones + 24 colored rungs across the 6 genes; rung brightness scales with expression.
2. **Per-gene bars** (Move Speed, Metabolism, Fertility, Longevity, Vision, Strength) with current numerical value.
3. A **composite fitness index** combining the genes (lower metabolism is better since it cuts energy drain).

## Notes for Next Iteration

1. Add spatial partitioning for mate/food search (perf at 1000+ agents).
2. Add carnivore species + predator/prey dynamics.
3. Persist timeline snapshots to CSV/JSON.
4. Add tribes / territories that emerge from clustering.
