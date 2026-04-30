#pragma once

#include <vector>

#include "AnimationSystem.h"
#include "SimulationTypes.h"
#include "WorldSystem.h"

class CivilizationSystem {
public:
    CivilizationSystem(float terrainBaseY, int halfTiles, float tileSize);

    void InitializePopulation(int initialCount, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);
    void Update(float dt, std::vector<FoodNode>& foodNodes, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);

    const std::vector<Agent>& Agents() const;
    const CivilizationStats& Stats() const;

    int FindAgentNearestTo(const Vector3& worldPos, float maxDistance) const;
    const Agent* FindAgentById(int id) const;

private:
    Agent CreateRandomAgent(int generation = 0);
    Agent CreateChild(const Agent& a, const Agent& b);
    void UpdateAgentContinuous(Agent& agent, float dt, std::vector<FoodNode>& foodNodes, const AnimationClip& idleClip, const AnimationClip& walkClip, const AnimationClip& runClip);
    void SimStep();
    void ResolveReproduction();
    void CleanupDead();

    int FindNearestFood(const Agent& agent, const std::vector<FoodNode>& foodNodes, float radius) const;
    int FindNearestWater(const Agent& agent, const std::vector<FoodNode>& foodNodes, float radius) const;
    int FindNearestMate(const Agent& agent, float radius) const;
    int FindAgentIndexById(int id) const;

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

    int carryingCapacity_ = 1400;
    int hardPopulationCap_ = 2400;
};
