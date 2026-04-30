#include "CivilizationSystem.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "WorldSystem.h"

namespace {

float Clamp01(float v) {
    return Clamp(v, 0.0f, 1.0f);
}

float DistXZ(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

bool PathCrossesWater(const Vector3& from, const Vector3& to, float terrainBaseY) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 0.001f) return false;
    const int samples = std::min(8, std::max(2, static_cast<int>(dist * 0.6f)));
    for (int i = 1; i < samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float sx = from.x + dx * t;
        const float sz = from.z + dz * t;
        if (IsWaterAt(sx, sz, terrainBaseY)) return true;
    }
    return false;
}

}  // namespace

CivilizationSystem::CivilizationSystem(float terrainBaseY, int halfTiles, float tileSize)
    : terrainBaseY_(terrainBaseY), halfTiles_(halfTiles), tileSize_(tileSize) {
    worldRadius_ = static_cast<float>(halfTiles_) * tileSize_ - 0.8f;
}

void CivilizationSystem::InitializePopulation(int initialCount, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip) {
    agents_.clear();
    agents_.reserve(std::max(initialCount * 2, 256));

    stats_ = CivilizationStats{};
    nextId_ = 1;

    SetRandomSeed(42);
    for (int i = 0; i < initialCount; ++i) {
        Agent agent = CreateRandomAgent(0);
        agent.activeClip = ChooseClip(idleClip, walkClip, runClip, agent.moveState);
        agents_.push_back(agent);
    }

    stats_.living = static_cast<int>(agents_.size());
    stats_.totalBorn = static_cast<int>(agents_.size());
}

void CivilizationSystem::Update(float dt, std::vector<FoodNode>& foodNodes, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip) {
    stats_.foragingAgents = 0;
    stats_.eatingAgents = 0;
    stats_.drinkingAgents = 0;
    stats_.matingSeekers = 0;

    for (Agent& agent : agents_) {
        if (!agent.alive) continue;
        UpdateAgentContinuous(agent, dt, foodNodes, idleClip, walkClip, runClip);
    }

    simAccumulator_ += dt;
    while (simAccumulator_ >= simStepSeconds_) {
        SimStep();
        simAccumulator_ -= simStepSeconds_;
    }
}

const std::vector<Agent>& CivilizationSystem::Agents() const {
    return agents_;
}

const CivilizationStats& CivilizationSystem::Stats() const {
    return stats_;
}

int CivilizationSystem::FindAgentNearestTo(const Vector3& worldPos, float maxDistance) const {
    int bestId = -1;
    float bestDist = maxDistance;
    for (const Agent& a : agents_) {
        if (!a.alive) continue;
        const float d = DistXZ(a.position, worldPos);
        if (d < bestDist) {
            bestDist = d;
            bestId = a.id;
        }
    }
    return bestId;
}

const Agent* CivilizationSystem::FindAgentById(int id) const {
    for (const Agent& a : agents_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

Agent CivilizationSystem::CreateRandomAgent(int generation) {
    Agent agent;
    agent.id = nextId_++;
    agent.generation = generation;
    agent.sex = (GetRandomValue(0, 1) == 0) ? Sex::Female : Sex::Male;

    agent.genome.moveSpeed = RandomFloat(0.75f, 1.35f);
    agent.genome.metabolism = RandomFloat(0.8f, 1.35f);
    agent.genome.fertility = RandomFloat(0.8f, 1.3f);
    agent.genome.longevity = RandomFloat(0.8f, 1.3f);
    agent.genome.vision = RandomFloat(0.8f, 1.3f);
    agent.genome.strength = RandomFloat(0.8f, 1.3f);

    agent.maxAge = RandomFloat(65.0f, 95.0f) * agent.genome.longevity;
    agent.age = RandomFloat(0.0f, 28.0f);
    agent.energy = RandomFloat(0.6f, 1.0f);
    agent.hydration = RandomFloat(0.6f, 1.0f);

    agent.fertilityCooldown = RandomFloat(0.0f, 0.66f);

    int attempts = 0;
    do {
        agent.position = {
            static_cast<float>(GetRandomValue(-halfTiles_ + 3, halfTiles_ - 3)) * tileSize_,
            0.0f,
            static_cast<float>(GetRandomValue(-halfTiles_ + 3, halfTiles_ - 3)) * tileSize_
        };
        ++attempts;
    } while (IsWaterAt(agent.position.x, agent.position.z, terrainBaseY_) && attempts < 10);
    agent.position.y = TerrainHeight(agent.position.x, agent.position.z, terrainBaseY_) + 0.74f;

    agent.yaw = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
    agent.directionChangeInterval = RandomFloat(1.8f, 4.5f);

    return agent;
}

Agent CivilizationSystem::CreateChild(const Agent& a, const Agent& b) {
    Agent child;
    child.id = nextId_++;
    child.generation = std::max(a.generation, b.generation) + 1;
    child.sex = (GetRandomValue(0, 1) == 0) ? Sex::Female : Sex::Male;

    auto mixGene = [this](float ga, float gb) {
        float mixed = (ga + gb) * 0.5f;
        if (RandomFloat(0.0f, 1.0f) < 0.10f) {
            mixed += RandomFloat(-0.08f, 0.08f);
        }
        return std::max(0.5f, std::min(1.6f, mixed));
    };

    child.genome.moveSpeed = mixGene(a.genome.moveSpeed, b.genome.moveSpeed);
    child.genome.metabolism = mixGene(a.genome.metabolism, b.genome.metabolism);
    child.genome.fertility = mixGene(a.genome.fertility, b.genome.fertility);
    child.genome.longevity = mixGene(a.genome.longevity, b.genome.longevity);
    child.genome.vision = mixGene(a.genome.vision, b.genome.vision);
    child.genome.strength = mixGene(a.genome.strength, b.genome.strength);

    child.maxAge = RandomFloat(65.0f, 95.0f) * child.genome.longevity;
    child.age = 0.0f;
    child.energy = 0.75f;
    child.hydration = 0.85f;
    child.fertilityCooldown = RandomFloat(0.33f, 1.0f);
    child.directionChangeInterval = RandomFloat(1.8f, 4.5f);

    child.position = {
        (a.position.x + b.position.x) * 0.5f + RandomFloat(-0.4f, 0.4f),
        0.0f,
        (a.position.z + b.position.z) * 0.5f + RandomFloat(-0.4f, 0.4f)
    };
    child.position.x = Clamp(child.position.x, -worldRadius_, worldRadius_);
    child.position.z = Clamp(child.position.z, -worldRadius_, worldRadius_);
    child.position.y = TerrainHeight(child.position.x, child.position.z, terrainBaseY_) + 0.74f;

    child.yaw = RandomFloat(0.0f, 2.0f * PI);

    return child;
}

int CivilizationSystem::FindNearestFood(const Agent& agent, const std::vector<FoodNode>& foodNodes, float radius) const {
    int best = -1;
    float bestD = radius;
    for (int i = 0; i < static_cast<int>(foodNodes.size()); ++i) {
        const FoodNode& node = foodNodes[static_cast<size_t>(i)];
        if (node.kind == FoodKind::Water) continue;
        if (node.amount < 0.05f) continue;
        const float d = DistXZ(agent.position, node.position);
        if (d >= bestD) continue;
        if (PathCrossesWater(agent.position, node.position, terrainBaseY_)) continue;
        bestD = d;
        best = i;
    }
    return best;
}

int CivilizationSystem::FindNearestWater(const Agent& agent, const std::vector<FoodNode>& foodNodes, float radius) const {
    int best = -1;
    float bestD = radius;
    for (int i = 0; i < static_cast<int>(foodNodes.size()); ++i) {
        const FoodNode& node = foodNodes[static_cast<size_t>(i)];
        if (node.kind != FoodKind::Water) continue;
        const float d = DistXZ(agent.position, node.position);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

int CivilizationSystem::FindAgentIndexById(int id) const {
    for (int i = 0; i < static_cast<int>(agents_.size()); ++i) {
        if (agents_[static_cast<size_t>(i)].id == id) return i;
    }
    return -1;
}

int CivilizationSystem::FindNearestMate(const Agent& agent, float radius) const {
    int bestId = -1;
    float bestD = radius;
    for (const Agent& other : agents_) {
        if (!other.alive) continue;
        if (other.id == agent.id) continue;
        if (other.sex == agent.sex) continue;
        const bool mature = other.age > 8.0f && other.age < other.maxAge * 0.84f;
        if (!mature) continue;
        if (other.fertilityCooldown > 0.0f) continue;
        if (other.energy < 0.5f || other.hydration < 0.4f) continue;
        const float d = DistXZ(agent.position, other.position);
        if (d >= bestD) continue;
        if (PathCrossesWater(agent.position, other.position, terrainBaseY_)) continue;
        bestD = d;
        bestId = other.id;
    }
    return bestId;
}

void CivilizationSystem::UpdateAgentContinuous(
    Agent& agent,
    float dt,
    std::vector<FoodNode>& foodNodes,
    const AnimationClip& idleClip,
    const AnimationClip& walkClip,
    const AnimationClip& runClip) {
    agent.moveTimer += dt;
    agent.behaviorTimer += dt;

    const float visionRadius = 14.0f * agent.genome.vision;
    const float hungerCritical = 0.35f;   // urgent food.
    const float hungerLow = 0.55f;        // start looking opportunistically.
    const float thirstCritical = 0.30f;   // urgent water.
    const float thirstLow = 0.50f;
    const float mateEnergyMin = 0.65f;    // healthy enough to court.
    const float mateHydrationMin = 0.55f;
    const float interactRange = 1.1f;

    const bool mature = agent.age > 8.0f && agent.age < agent.maxAge * 0.84f;
    const bool fertileNow = mature && agent.fertilityCooldown <= 0.0f;

    if (agent.behavior == Behavior::Eat || agent.behavior == Behavior::Drink) {
        if (agent.behaviorTimer > 1.8f) {
            agent.behavior = Behavior::Wander;
            agent.behaviorTimer = 0.0f;
            agent.targetFoodIndex = -1;
            agent.targetWaterIndex = -1;
        }
    } else {
        // Priority ladder.
        if (agent.hydration < thirstCritical) {
            agent.behavior = Behavior::SeekWater;
        } else if (agent.energy < hungerCritical) {
            agent.behavior = Behavior::SeekFood;
        } else if (fertileNow && agent.energy >= mateEnergyMin && agent.hydration >= mateHydrationMin) {
            agent.behavior = Behavior::SeekMate;
        } else if (agent.energy < hungerLow) {
            agent.behavior = Behavior::SeekFood;
        } else if (agent.hydration < thirstLow) {
            agent.behavior = Behavior::SeekWater;
        } else {
            agent.behavior = Behavior::Wander;
        }
    }

    // Acquire targets.
    if (agent.behavior == Behavior::SeekFood) {
        if (agent.targetFoodIndex < 0 ||
            agent.targetFoodIndex >= static_cast<int>(foodNodes.size()) ||
            foodNodes[static_cast<size_t>(agent.targetFoodIndex)].amount < 0.05f) {
            agent.targetFoodIndex = FindNearestFood(agent, foodNodes, visionRadius);
        }
        if (agent.targetFoodIndex < 0) agent.behavior = Behavior::Wander;
    } else if (agent.behavior == Behavior::SeekWater) {
        if (agent.targetWaterIndex < 0 ||
            agent.targetWaterIndex >= static_cast<int>(foodNodes.size())) {
            agent.targetWaterIndex = FindNearestWater(agent, foodNodes, visionRadius * 1.8f);
        }
        if (agent.targetWaterIndex < 0) agent.behavior = Behavior::Wander;
    } else if (agent.behavior == Behavior::SeekMate) {
        if (agent.targetMateId >= 0) {
            const int idx = FindAgentIndexById(agent.targetMateId);
            if (idx < 0 || !agents_[static_cast<size_t>(idx)].alive ||
                agents_[static_cast<size_t>(idx)].fertilityCooldown > 0.0f) {
                agent.targetMateId = -1;
            }
        }
        if (agent.targetMateId < 0) {
            agent.targetMateId = FindNearestMate(agent, visionRadius);
        }
        if (agent.targetMateId < 0) agent.behavior = Behavior::Wander;
    }

    // Apply behavior.
    switch (agent.behavior) {
        case Behavior::SeekFood: {
            const FoodNode& node = foodNodes[static_cast<size_t>(agent.targetFoodIndex)];
            const float d = DistXZ(agent.position, node.position);
            const Vector3 toFood = {node.position.x - agent.position.x, 0.0f, node.position.z - agent.position.z};
            agent.yaw = std::atan2(toFood.z, toFood.x);
            const float speed = 2.6f * agent.genome.moveSpeed;
            agent.velocity = {std::cos(agent.yaw) * speed, 0.0f, std::sin(agent.yaw) * speed};
            agent.moveState = MoveState::Walk;
            stats_.foragingAgents += 1;

            if (d < interactRange) {
                agent.behavior = Behavior::Eat;
                agent.behaviorTimer = 0.0f;
                agent.velocity = {0.0f, 0.0f, 0.0f};
                agent.moveState = MoveState::Idle;
            }
            break;
        }
        case Behavior::SeekWater: {
            const FoodNode& node = foodNodes[static_cast<size_t>(agent.targetWaterIndex)];
            const float d = DistXZ(agent.position, node.position);
            const Vector3 toWater = {node.position.x - agent.position.x, 0.0f, node.position.z - agent.position.z};
            agent.yaw = std::atan2(toWater.z, toWater.x);
            const float speed = 2.4f * agent.genome.moveSpeed;
            agent.velocity = {std::cos(agent.yaw) * speed, 0.0f, std::sin(agent.yaw) * speed};
            agent.moveState = MoveState::Walk;
            stats_.foragingAgents += 1;

            // Stop slightly before water edge to avoid wading.
            if (d < interactRange + 0.4f) {
                agent.behavior = Behavior::Drink;
                agent.behaviorTimer = 0.0f;
                agent.velocity = {0.0f, 0.0f, 0.0f};
                agent.moveState = MoveState::Idle;
            }
            break;
        }
        case Behavior::Eat: {
            if (agent.targetFoodIndex >= 0 && agent.targetFoodIndex < static_cast<int>(foodNodes.size())) {
                FoodNode& node = foodNodes[static_cast<size_t>(agent.targetFoodIndex)];
                const float bite = 0.35f * agent.genome.strength * dt;
                const float consumed = std::min(node.amount, bite);
                node.amount -= consumed;
                agent.energy = Clamp01(agent.energy + consumed * 0.9f);
                if (node.amount < 0.02f || agent.energy > 0.95f) {
                    agent.behavior = Behavior::Wander;
                    agent.behaviorTimer = 0.0f;
                    agent.targetFoodIndex = -1;
                }
            } else {
                agent.behavior = Behavior::Wander;
                agent.targetFoodIndex = -1;
            }
            agent.velocity = {0.0f, 0.0f, 0.0f};
            agent.moveState = MoveState::Idle;
            stats_.eatingAgents += 1;
            break;
        }
        case Behavior::Drink: {
            const float gulp = 0.55f * agent.genome.strength * dt;
            agent.hydration = Clamp01(agent.hydration + gulp);
            if (agent.hydration > 0.97f) {
                agent.behavior = Behavior::Wander;
                agent.behaviorTimer = 0.0f;
                agent.targetWaterIndex = -1;
            }
            agent.velocity = {0.0f, 0.0f, 0.0f};
            agent.moveState = MoveState::Idle;
            stats_.drinkingAgents += 1;
            break;
        }
        case Behavior::SeekMate: {
            const int mateIdx = FindAgentIndexById(agent.targetMateId);
            if (mateIdx < 0) {
                agent.behavior = Behavior::Wander;
                agent.targetMateId = -1;
                break;
            }
            const Agent& mate = agents_[static_cast<size_t>(mateIdx)];
            const Vector3 toMate = {mate.position.x - agent.position.x, 0.0f, mate.position.z - agent.position.z};
            agent.yaw = std::atan2(toMate.z, toMate.x);
            const float speed = 2.4f * agent.genome.moveSpeed;
            agent.velocity = {std::cos(agent.yaw) * speed, 0.0f, std::sin(agent.yaw) * speed};
            agent.moveState = MoveState::Walk;
            stats_.matingSeekers += 1;
            // ResolveReproduction picks pairs each tick; we just ensure proximity.
            const float d = DistXZ(agent.position, mate.position);
            if (d < 1.4f) {
                // Settle next to partner; reproduction tick will handle birth.
                agent.velocity = {0.0f, 0.0f, 0.0f};
                agent.moveState = MoveState::Idle;
            }
            break;
        }
        case Behavior::Wander:
        case Behavior::Rest:
        default: {
            if (agent.moveTimer >= agent.directionChangeInterval) {
                agent.moveTimer = 0.0f;
                agent.directionChangeInterval = RandomFloat(1.8f, 4.5f);
                if (GetRandomValue(0, 100) < 72) {
                    agent.yaw = RandomFloat(0.0f, 2.0f * PI);
                }
            }

            const float movePhase = agent.moveTimer / std::max(agent.directionChangeInterval, 0.0001f);
            const bool isWalking = movePhase < 0.7f && agent.energy > 0.10f;
            if (isWalking) {
                const Vector3 dir = {std::cos(agent.yaw), 0.0f, std::sin(agent.yaw)};
                const float speed = 1.9f * agent.genome.moveSpeed;
                agent.velocity = Vector3Scale(dir, speed);
                agent.moveState = MoveState::Walk;
            } else {
                agent.velocity = {0.0f, 0.0f, 0.0f};
                agent.moveState = MoveState::Idle;
            }
            break;
        }
    }

    agent.position.x += agent.velocity.x * dt;
    agent.position.z += agent.velocity.z * dt;

    agent.position.x = Clamp(agent.position.x, -worldRadius_, worldRadius_);
    agent.position.z = Clamp(agent.position.z, -worldRadius_, worldRadius_);

    // Avoid wading deep into water unless drinking.
    if (agent.behavior != Behavior::Drink && agent.behavior != Behavior::SeekWater) {
        if (IsWaterAt(agent.position.x, agent.position.z, terrainBaseY_)) {
            // Walk back along incoming velocity until we leave water (max ~1.5 units).
            const float vx = agent.velocity.x;
            const float vz = agent.velocity.z;
            const float vlen = std::sqrt(vx * vx + vz * vz);
            const float nx = (vlen > 0.0001f) ? vx / vlen : std::cos(agent.yaw);
            const float nz = (vlen > 0.0001f) ? vz / vlen : std::sin(agent.yaw);
            for (int step = 0; step < 12; ++step) {
                agent.position.x -= nx * 0.18f;
                agent.position.z -= nz * 0.18f;
                agent.position.x = Clamp(agent.position.x, -worldRadius_, worldRadius_);
                agent.position.z = Clamp(agent.position.z, -worldRadius_, worldRadius_);
                if (!IsWaterAt(agent.position.x, agent.position.z, terrainBaseY_)) break;
            }
            // Drop unreachable targets across the water; resume Wander away from shore.
            agent.targetFoodIndex = -1;
            agent.targetMateId = -1;
            agent.behavior = Behavior::Wander;
            agent.behaviorTimer = 0.0f;
            agent.moveTimer = 0.0f;
            agent.directionChangeInterval = RandomFloat(2.5f, 4.5f);
            // Face away from where we came.
            agent.yaw = std::atan2(-nz, -nx);
            agent.velocity = {0.0f, 0.0f, 0.0f};
            agent.moveState = MoveState::Idle;
        }
    }
    agent.position.y = TerrainHeight(agent.position.x, agent.position.z, terrainBaseY_) + 0.74f;

    agent.activeClip = ChooseClip(idleClip, walkClip, runClip, agent.moveState);
    if (agent.activeClip != nullptr && !agent.activeClip->frames.empty()) {
        agent.animTime += dt * agent.activeClip->fps;
        const int frameCount = static_cast<int>(agent.activeClip->frames.size());
        agent.animFrameIndex = static_cast<int>(agent.animTime) % frameCount;
    } else {
        agent.animFrameIndex = 0;
    }

    if (Vector3Length(agent.velocity) > 0.01f) {
        agent.faceLeft = std::sin(agent.yaw) < 0.0f;
    }
}

void CivilizationSystem::SimStep() {
    stats_.tick += 1;
    stats_.simTimeSeconds += simStepSeconds_;
    const float simStepYears = simStepSeconds_ / secondsPerYear_;
    stats_.simTimeYears += simStepYears;

    const int livingNow = static_cast<int>(std::count_if(agents_.begin(), agents_.end(), [](const Agent& a) { return a.alive; }));
    const float pressure = std::max(0.0f, static_cast<float>(livingNow - carryingCapacity_)) / static_cast<float>(std::max(carryingCapacity_, 1));
    stats_.populationPressure = pressure;

    for (Agent& agent : agents_) {
        if (!agent.alive) continue;

        agent.age += simStepYears;
        agent.fertilityCooldown = std::max(0.0f, agent.fertilityCooldown - simStepYears);

        // Energy drain - reduced to give monkeys time to gather.
        float drain = (0.0055f + 0.0095f * agent.genome.metabolism) * simStepSeconds_;
        drain += Vector3Length(agent.velocity) * 0.0010f * simStepSeconds_;
        drain += pressure * 0.0090f * simStepSeconds_;
        agent.energy = Clamp01(agent.energy - drain);

        // Hydration drains separately, slightly slower than energy.
        float hydrationDrain = (0.0045f + 0.0075f * agent.genome.metabolism) * simStepSeconds_;
        hydrationDrain += Vector3Length(agent.velocity) * 0.0011f * simStepSeconds_;
        agent.hydration = Clamp01(agent.hydration - hydrationDrain);

        if (agent.age > agent.maxAge) {
            agent.alive = false;
            agent.deathCause = DeathCause::Age;
            stats_.deathsThisEpoch += 1;
            stats_.totalDead += 1;
            stats_.ageDeaths += 1;
        } else if (agent.energy <= 0.001f || agent.hydration <= 0.001f) {
            agent.alive = false;
            agent.deathCause = DeathCause::Starvation;
            stats_.deathsThisEpoch += 1;
            stats_.totalDead += 1;
            stats_.starvationDeaths += 1;
        }
    }

    ResolveReproduction();
    CleanupDead();

    stats_.living = static_cast<int>(std::count_if(agents_.begin(), agents_.end(), [](const Agent& a) { return a.alive; }));
    for (const Agent& agent : agents_) {
        if (agent.alive) stats_.maxGeneration = std::max(stats_.maxGeneration, agent.generation);
    }

    const int currentYear = static_cast<int>(std::floor(stats_.simTimeYears));
    if (currentYear > stats_.epoch) {
        stats_.lastEpochBirths = stats_.birthsThisEpoch;
        stats_.lastEpochDeaths = stats_.deathsThisEpoch;
        stats_.birthsThisEpoch = 0;
        stats_.deathsThisEpoch = 0;
        stats_.epoch = currentYear;
    }
}

void CivilizationSystem::ResolveReproduction() {
    if (static_cast<int>(agents_.size()) >= hardPopulationCap_) return;

    stats_.fertileFemales = 0;
    stats_.fertileMales = 0;
    stats_.reproductionChecks = 0;
    stats_.reproductionSuccesses = 0;

    std::vector<int> females;
    std::vector<int> males;
    females.reserve(agents_.size());
    males.reserve(agents_.size());

    for (int i = 0; i < static_cast<int>(agents_.size()); ++i) {
        const Agent& a = agents_[static_cast<size_t>(i)];
        if (!a.alive) continue;
        const bool mature = a.age > 8.0f && a.age < a.maxAge * 0.84f;
        const bool fertile = mature && a.fertilityCooldown <= 0.0f && a.energy > 0.45f && a.hydration > 0.35f;
        if (!fertile) continue;

        if (a.sex == Sex::Female) {
            females.push_back(i);
            stats_.fertileFemales += 1;
        } else {
            males.push_back(i);
            stats_.fertileMales += 1;
        }
    }

    for (int fi : females) {
        if (static_cast<int>(agents_.size()) >= hardPopulationCap_) break;
        Agent& female = agents_[static_cast<size_t>(fi)];
        if (!female.alive || female.fertilityCooldown > 0.0f || female.energy <= 0.45f) continue;

        int bestMale = -1;
        float bestDist = 6.5f;

        for (int mi : males) {
            if (mi == fi) continue;
            const Agent& male = agents_[static_cast<size_t>(mi)];
            if (!male.alive || male.fertilityCooldown > 0.0f || male.energy <= 0.45f) continue;

            const float d = DistanceXZ(female, male);
            if (d < bestDist) {
                bestDist = d;
                bestMale = mi;
            }
        }

        if (bestMale < 0) continue;

        Agent& male = agents_[static_cast<size_t>(bestMale)];
        if (!male.alive || male.fertilityCooldown > 0.0f) continue;

        stats_.reproductionChecks += 1;

        const float fertilityFactor = (female.genome.fertility + male.genome.fertility) * 0.5f;
        const float chance = Clamp(0.28f * fertilityFactor, 0.10f, 0.55f);
        if (RandomFloat(0.0f, 1.0f) > chance) continue;

        Agent child = CreateChild(female, male);
        agents_.push_back(child);

        female.energy = Clamp01(female.energy - 0.22f);
        male.energy = Clamp01(male.energy - 0.18f);
        female.fertilityCooldown = RandomFloat(0.33f, 2.0f);
        male.fertilityCooldown = RandomFloat(0.33f, 2.0f);
        female.childrenBorn += 1;
        male.childrenBorn += 1;

        stats_.birthsThisEpoch += 1;
        stats_.totalBorn += 1;
        stats_.maxGeneration = std::max(stats_.maxGeneration, child.generation);
        stats_.reproductionSuccesses += 1;
    }
}

void CivilizationSystem::CleanupDead() {
    agents_.erase(
        std::remove_if(agents_.begin(), agents_.end(), [](const Agent& a) {
            return !a.alive;
        }),
        agents_.end());
}

float CivilizationSystem::RandomFloat(float minValue, float maxValue) const {
    const float unit = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
    return minValue + (maxValue - minValue) * unit;
}

float CivilizationSystem::DistanceXZ(const Agent& a, const Agent& b) const {
    const float dx = a.position.x - b.position.x;
    const float dz = a.position.z - b.position.z;
    return std::sqrt(dx * dx + dz * dz);
}
