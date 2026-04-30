#pragma once

#include <string>

#include "raylib.h"

struct AnimationClip;

enum class MoveState {
    Idle,
    Walk,
    Run
};

enum class Sex {
    Female,
    Male
};

enum class DeathCause {
    None,
    Age,
    Starvation
};

enum class Behavior {
    Wander,
    SeekFood,
    Eat,
    SeekWater,
    Drink,
    SeekMate,
    Rest,
    Migrate
};

struct Genome {
    float moveSpeed = 1.0f;   // Multiplier for base movement speed.
    float metabolism = 1.0f;  // Higher means faster energy drain.
    float fertility = 1.0f;   // Higher means better reproduction chance.
    float longevity = 1.0f;   // Higher means older max age.
    float vision = 1.0f;      // Search radius for food/water.
    float strength = 1.0f;    // Eating/drinking efficiency.
};

struct Agent {
    int id = -1;
    bool alive = true;
    int generation = 0;
    Sex sex = Sex::Female;
    DeathCause deathCause = DeathCause::None;

    float age = 0.0f;
    float maxAge = 80.0f;
    float energy = 1.0f;
    float hydration = 1.0f;
    float fertilityCooldown = 0.0f;

    Genome genome;

    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 velocity = {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float moveTimer = 0.0f;
    float directionChangeInterval = 3.0f;
    MoveState moveState = MoveState::Idle;
    Behavior behavior = Behavior::Wander;
    int targetFoodIndex = -1;
    int targetWaterIndex = -1;
    int targetMateId = -1;
    float behaviorTimer = 0.0f;

    int childrenBorn = 0;
    int nationalityId = -1;
    float ancestry[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    Vector3 migrateTarget = {0.0f, 0.0f, 0.0f};
    bool isSwimming = false;

    const AnimationClip* activeClip = nullptr;
    float animTime = 0.0f;
    int animFrameIndex = 0;
    bool faceLeft = false;
};

struct CivilizationStats {
    int living = 0;
    int totalBorn = 0;
    int totalDead = 0;
    int birthsThisEpoch = 0;
    int deathsThisEpoch = 0;
    int lastEpochBirths = 0;
    int lastEpochDeaths = 0;
    int maxGeneration = 0;
    float simTimeSeconds = 0.0f;
    float simTimeYears = 0.0f;
    int tick = 0;
    int epoch = 0;  // Epoch is intentionally equal to current year index.

    // Live diagnostics for reproduction debugging.
    int fertileFemales = 0;
    int fertileMales = 0;
    int reproductionChecks = 0;
    int reproductionSuccesses = 0;
    float populationPressure = 0.0f;

    // Resource diagnostics.
    int foragingAgents = 0;
    int eatingAgents = 0;
    int drinkingAgents = 0;
    int matingSeekers = 0;
    int starvationDeaths = 0;
    int ageDeaths = 0;
};
