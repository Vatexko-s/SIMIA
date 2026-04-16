#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "SimulationTypes.h"

struct AnimationClip {
    std::vector<Texture2D> frames;
    float fps = 8.0f;
    std::string name;
};

std::vector<std::filesystem::path> CollectPngFrames(const std::filesystem::path& dir);
std::filesystem::path ResolveExistingPath(const std::vector<std::string>& relativeCandidates);
AnimationClip LoadClip(const std::vector<std::string>& candidateDirs, float fps, const char* name);
void UnloadClip(AnimationClip& clip);
const AnimationClip* ChooseClip(const AnimationClip& idle, const AnimationClip& walk, const AnimationClip& run, MoveState state);
const char* MoveStateLabel(MoveState state);

