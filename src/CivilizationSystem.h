#pragma once

#include <vector>

#include "AnimationSystem.h"
#include "SimulationTypes.h"

class CivilizationSystem {
public:
    CivilizationSystem(float terrainBaseY, int halfTiles, float tileSize);

    void InitializePopulation(int initialCount, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);
    void Update(float dt, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);

    const std::vector<Agent>& Agents() const;
    const CivilizationStats& Stats() const;

private:
    Agent CreateRandomAgent(int generation = 0);
    Agent CreateChild(const Agent& a, const Agent& b);
    void UpdateAgentContinuous(Agent& agent, float dt, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);
    void SimStep();
    void ResolveReproduction();
    void CleanupDead();

    float RandomFloat(float minValue, float maxValue) const;
    float DistanceXZ(const Agent& a, const Agent& b) const;

private:
    std::vector<Agent> agents_;
    CivilizationStats stats_;

    float terrainBaseY_ = -0.55f;
    int halfTiles_ = 35;
    float tileSize_ = 1.2f;
    float worldRadius_ = 40.0f;

    int nextId_ = 1;

    float simAccumulator_ = 0.0f;
    float simStepSeconds_ = 0.1f;
    float secondsPerYear_ = 3.0f;

    int carryingCapacity_ = 700;
    int hardPopulationCap_ = 1200;
};

