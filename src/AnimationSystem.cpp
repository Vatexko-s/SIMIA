#include "AnimationSystem.h"

#include <algorithm>

std::vector<std::filesystem::path> CollectPngFrames(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return files;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".png" || entry.path().extension() == ".PNG") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

std::filesystem::path ResolveExistingPath(const std::vector<std::string>& relativeCandidates) {
    for (const std::string& rel : relativeCandidates) {
        std::filesystem::path p0 = std::filesystem::path(rel);
        std::filesystem::path p1 = std::filesystem::path("..") / rel;
        std::filesystem::path p2 = std::filesystem::path("..") / ".." / rel;
        if (std::filesystem::exists(p0)) return p0;
        if (std::filesystem::exists(p1)) return p1;
        if (std::filesystem::exists(p2)) return p2;
    }

    return {};
}

AnimationClip LoadClip(const std::vector<std::string>& candidateDirs, float fps, const char* name) {
    AnimationClip clip;
    clip.fps = fps;
    clip.name = name;

    const std::filesystem::path directory = ResolveExistingPath(candidateDirs);
    if (directory.empty()) return clip;

    const std::vector<std::filesystem::path> framePaths = CollectPngFrames(directory);
    for (const auto& framePath : framePaths) {
        Texture2D frame = LoadTexture(framePath.string().c_str());
        if (frame.id != 0) clip.frames.push_back(frame);
    }

    return clip;
}

void UnloadClip(AnimationClip& clip) {
    for (Texture2D& frame : clip.frames) {
        if (frame.id != 0) UnloadTexture(frame);
    }
    clip.frames.clear();
}

const AnimationClip* ChooseClip(const AnimationClip& idle, const AnimationClip& walk, const AnimationClip& run, MoveState state) {
    switch (state) {
        case MoveState::Run:
            if (!run.frames.empty()) return &run;
            if (!walk.frames.empty()) return &walk;
            return &idle;
        case MoveState::Walk:
            if (!walk.frames.empty()) return &walk;
            return &idle;
        case MoveState::Idle:
        default:
            return &idle;
    }
}

const char* MoveStateLabel(MoveState state) {
    switch (state) {
        case MoveState::Idle:
            return "Idle";
        case MoveState::Walk:
            return "Walk";
        case MoveState::Run:
            return "Run";
        default:
            return "Unknown";
    }
}

