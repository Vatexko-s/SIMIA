#include "CivilizationSystem.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "WorldSystem.h"

namespace {

float Clamp01(float v) {
    return Clamp(v, 0.0f, 1.0f);
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

void CivilizationSystem::Update(float dt, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip) {
    for (Agent& agent : agents_) {
        if (!agent.alive) continue;
        UpdateAgentContinuous(agent, dt, idleClip, walkClip, runClip);
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

Agent CivilizationSystem::CreateRandomAgent(int generation) {
    Agent agent;
    agent.id = nextId_++;
    agent.generation = generation;
    agent.sex = (GetRandomValue(0, 1) == 0) ? Sex::Female : Sex::Male;

    agent.genome.moveSpeed = RandomFloat(0.75f, 1.35f);
    agent.genome.metabolism = RandomFloat(0.8f, 1.35f);
    agent.genome.fertility = RandomFloat(0.8f, 1.3f);
    agent.genome.longevity = RandomFloat(0.8f, 1.3f);

    agent.maxAge = RandomFloat(65.0f, 95.0f) * agent.genome.longevity;
    agent.age = RandomFloat(0.0f, 28.0f);
    agent.energy = RandomFloat(0.45f, 1.0f);

    // Initial agents become reproductive quickly.
    agent.fertilityCooldown = RandomFloat(0.0f, 0.66f);  // ~0-2 seconds

    agent.position = {
        static_cast<float>(GetRandomValue(-halfTiles_ + 3, halfTiles_ - 3)) * tileSize_,
        0.0f,
        static_cast<float>(GetRandomValue(-halfTiles_ + 3, halfTiles_ - 3)) * tileSize_
    };
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

    child.maxAge = RandomFloat(65.0f, 95.0f) * child.genome.longevity;
    child.age = 0.0f;
    child.energy = 0.75f;
    // New agents can reproduce after a short interval.
    child.fertilityCooldown = RandomFloat(0.33f, 1.0f);  // ~1-3 seconds
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

void CivilizationSystem::UpdateAgentContinuous(
    Agent& agent,
    float dt,
    const AnimationClip& idleClip,
    const AnimationClip& walkClip,
    const AnimationClip& runClip) {
    agent.moveTimer += dt;

    if (agent.moveTimer >= agent.directionChangeInterval) {
        agent.moveTimer = 0.0f;
        agent.directionChangeInterval = RandomFloat(1.8f, 4.5f);

        const bool shouldMove = GetRandomValue(0, 100) < 72;
        if (shouldMove) {
            agent.yaw = RandomFloat(0.0f, 2.0f * PI);
        }
    }

    const float movePhase = (agent.moveTimer / std::max(agent.directionChangeInterval, 0.0001f));
    const bool isWalking = movePhase < 0.8f && agent.energy > 0.08f;

    if (isWalking) {
        const Vector3 dir = {std::cos(agent.yaw), 0.0f, std::sin(agent.yaw)};
        const float speed = 2.3f * agent.genome.moveSpeed;
        agent.velocity = Vector3Scale(dir, speed);
        agent.moveState = MoveState::Walk;
    } else {
        agent.velocity = {0.0f, 0.0f, 0.0f};
        agent.moveState = MoveState::Idle;
    }

    agent.position.x += agent.velocity.x * dt;
    agent.position.z += agent.velocity.z * dt;

    agent.position.x = Clamp(agent.position.x, -worldRadius_, worldRadius_);
    agent.position.z = Clamp(agent.position.z, -worldRadius_, worldRadius_);
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

        float drain = (0.010f + 0.022f * agent.genome.metabolism) * simStepSeconds_;
        drain += Vector3Length(agent.velocity) * 0.0018f * simStepSeconds_;
        drain += pressure * 0.015f * simStepSeconds_;

        agent.energy = Clamp01(agent.energy - drain);

        if (agent.age > agent.maxAge) {
            agent.alive = false;
            agent.deathCause = DeathCause::Age;
            stats_.deathsThisEpoch += 1;
            stats_.totalDead += 1;
        } else if (agent.energy <= 0.001f) {
            agent.alive = false;
            agent.deathCause = DeathCause::Starvation;
            stats_.deathsThisEpoch += 1;
            stats_.totalDead += 1;
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
        const bool fertile = mature && a.fertilityCooldown <= 0.0f && a.energy > 0.35f;
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
        if (!female.alive || female.fertilityCooldown > 0.0f || female.energy <= 0.35f) continue;

        int bestMale = -1;
        float bestDist = 6.5f;

        for (int mi : males) {
            if (mi == fi) continue;
            const Agent& male = agents_[static_cast<size_t>(mi)];
            if (!male.alive || male.fertilityCooldown > 0.0f || male.energy <= 0.35f) continue;

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
        // Requested breeding interval: 1 to 6 seconds max (1 year = 3 seconds).
        female.fertilityCooldown = RandomFloat(0.33f, 2.0f);
        male.fertilityCooldown = RandomFloat(0.33f, 2.0f);

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

