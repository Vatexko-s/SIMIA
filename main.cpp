#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "imgui.h"
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"

namespace {

constexpr int kScreenWidth = 1440;
constexpr int kScreenHeight = 900;
constexpr int kHalfTiles = 16;
constexpr float kTileSize = 1.2f;
constexpr float kTerrainBaseY = -0.55f;

struct WorldProp {
    Vector3 position;
    float scale;
    bool tree;
};

struct GameTuning {
    float moveSpeed = 6.0f;
    float sprintMultiplier = 1.75f;
    float acceleration = 14.0f;
    float cameraDistance = 22.0f;
    float cameraPitch = 36.0f;
    float cameraYaw = 45.0f;
    float spriteHeight = 2.15f;
    float spriteFootOffset = 0.03f;
    bool drawGrid = false;
    bool drawStats = true;
};

struct AnimationClip {
    std::vector<Texture2D> frames;
    float fps = 8.0f;
    std::string name;
};

enum class MoveState {
    Idle,
    Walk,
    Run
};

float TerrainHeight(float x, float z) {
    const float waves = std::sin(x * 0.22f) * 0.45f + std::cos(z * 0.19f) * 0.35f;
    const float details = std::sin((x + z) * 0.32f) * 0.22f + std::cos((x - z) * 0.27f) * 0.14f;
    return kTerrainBaseY + waves + details;
}

Color LerpColor(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        255
    };
}

void DrawWorldTile(float x, float z, float size) {
    const float h = TerrainHeight(x, z);
    const float waterLevel = -0.38f;
    const float pathBlend = std::exp(-std::fabs(x - z * 0.25f) * 0.19f);

    const Color grassLow = Color{93, 150, 72, 255};
    const Color grassHigh = Color{126, 183, 98, 255};
    const Color dirt = Color{131, 108, 82, 255};
    const Color water = Color{62, 128, 170, 255};

    const float heightTint = Clamp((h + 1.0f) / 1.5f, 0.0f, 1.0f);
    Color top = LerpColor(grassLow, grassHigh, heightTint);
    top = LerpColor(top, dirt, pathBlend * 0.55f);
    if (h <= waterLevel) top = LerpColor(water, top, 0.15f);

    const float thickness = h - kTerrainBaseY + 0.35f;
    const Vector3 tilePos = {x, kTerrainBaseY + thickness * 0.5f, z};
    DrawCubeV(tilePos, {size, thickness, size}, top);
    DrawCubeWiresV(tilePos, {size, thickness, size}, Fade(BLACK, 0.12f));

    if (h <= waterLevel + 0.03f) {
        DrawCubeV({x, waterLevel + 0.02f, z}, {size * 0.94f, 0.04f, size * 0.94f}, Fade(Color{95, 182, 220, 255}, 0.85f));
    }
}

std::vector<WorldProp> BuildProps() {
    std::vector<WorldProp> props;
    props.reserve(70);

    SetRandomSeed(1337);
    for (int i = 0; i < 70; ++i) {
        const float x = static_cast<float>(GetRandomValue(-kHalfTiles + 1, kHalfTiles - 1)) * kTileSize +
                        static_cast<float>(GetRandomValue(-35, 35)) * 0.01f;
        const float z = static_cast<float>(GetRandomValue(-kHalfTiles + 1, kHalfTiles - 1)) * kTileSize +
                        static_cast<float>(GetRandomValue(-35, 35)) * 0.01f;

        if (std::fabs(x - z * 0.25f) < 2.0f) continue;
        if (TerrainHeight(x, z) < -0.25f) continue;

        const bool tree = GetRandomValue(0, 100) > 35;
        const float scale = tree ? static_cast<float>(GetRandomValue(85, 135)) * 0.01f
                                 : static_cast<float>(GetRandomValue(55, 110)) * 0.01f;
        props.push_back({{x, TerrainHeight(x, z), z}, scale, tree});
    }

    return props;
}

void DrawProp(const WorldProp& prop) {
    if (prop.tree) {
        const float trunkHeight = 1.2f * prop.scale;
        const float trunkRadius = 0.15f * prop.scale;
        DrawCylinder({prop.position.x, prop.position.y + trunkHeight * 0.5f, prop.position.z}, trunkRadius, trunkRadius, trunkHeight, 8,
                     Color{114, 82, 56, 255});
        DrawSphere({prop.position.x, prop.position.y + trunkHeight + 0.55f * prop.scale, prop.position.z}, 0.75f * prop.scale,
                   Color{63, 125, 70, 255});
        DrawSphere({prop.position.x - 0.25f * prop.scale, prop.position.y + trunkHeight + 0.4f * prop.scale, prop.position.z + 0.2f * prop.scale},
                   0.55f * prop.scale, Color{77, 145, 80, 255});
    } else {
        DrawSphere({prop.position.x, prop.position.y + 0.28f * prop.scale, prop.position.z}, 0.35f * prop.scale, Color{118, 120, 126, 255});
    }
}

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
        const std::filesystem::path p0 = std::filesystem::path(rel);
        const std::filesystem::path p1 = std::filesystem::path("..") / rel;
        const std::filesystem::path p2 = std::filesystem::path("..") / ".." / rel;
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

}  // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kScreenWidth, kScreenHeight, "SIMIA - Isometric Adventure Template");
    SetExitKey(KEY_NULL);  // Avoid accidental Escape-based app exit.
    SetTargetFPS(144);

    rlImGuiSetup(true);

    Camera3D camera = {};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 32.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    AnimationClip idleClip = LoadClip(
        {"assets/NUDE_MONKEY/01-Idle/01-Idle", "assets/NUDE_MONKEY/Idle", "SIMIA/assets/NUDE_MONKEY/01-Idle/01-Idle"},
        8.0f,
        "Idle");
    AnimationClip walkClip = LoadClip(
        {"assets/NUDE_MONKEY/03-Walk/01-Walk", "assets/NUDE_MONKEY/Walk", "SIMIA/assets/NUDE_MONKEY/03-Walk/01-Walk"},
        11.0f,
        "Walk");
    AnimationClip runClip = LoadClip(
        {"assets/NUDE_MONKEY/04-Run", "assets/NUDE_MONKEY/Run", "SIMIA/assets/NUDE_MONKEY/04-Run"},
        14.0f,
        "Run");

    GameTuning tuning;
    std::vector<WorldProp> props = BuildProps();

    Vector3 playerPosition = {0.0f, TerrainHeight(0.0f, 0.0f) + 0.75f, 0.0f};
    Vector3 velocity = {};
    float playerYaw = 0.0f;
    float stamina = 1.0f;

    MoveState moveState = MoveState::Idle;
    const AnimationClip* activeClip = &idleClip;
    float animTime = 0.0f;
    int animFrameIndex = 0;
    bool faceLeft = false;

    while (!WindowShouldClose()) {
        const bool cmdQ = (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) && IsKeyPressed(KEY_Q);
        if (cmdQ) break;

        const float dt = GetFrameTime();

        Vector3 inputDir = {};
        if (IsKeyDown(KEY_W)) inputDir.z += 1.0f;
        if (IsKeyDown(KEY_S)) inputDir.z -= 1.0f;
        if (IsKeyDown(KEY_A)) inputDir.x -= 1.0f;
        if (IsKeyDown(KEY_D)) inputDir.x += 1.0f;
        if (Vector3Length(inputDir) > 0.0f) inputDir = Vector3Normalize(inputDir);

        const float yaw = DEG2RAD * tuning.cameraYaw;
        const float pitch = DEG2RAD * tuning.cameraPitch;

        // Inverted movement fix: use camera-forward direction from camera to target (not the opposite).
        Vector3 camForward = {-std::cos(yaw), 0.0f, -std::sin(yaw)};
        Vector3 camRight = {-camForward.z, 0.0f, camForward.x};

        Vector3 moveWorld = Vector3Add(Vector3Scale(camForward, inputDir.z), Vector3Scale(camRight, inputDir.x));
        if (Vector3Length(moveWorld) > 0.0f) moveWorld = Vector3Normalize(moveWorld);

        const bool sprint = IsKeyDown(KEY_LEFT_SHIFT) && stamina > 0.1f;
        const float speed = tuning.moveSpeed * (sprint ? tuning.sprintMultiplier : 1.0f);
        Vector3 desiredVel = Vector3Scale(moveWorld, speed);
        velocity = Vector3Lerp(velocity, desiredVel, Clamp(dt * tuning.acceleration, 0.0f, 1.0f));

        playerPosition.x += velocity.x * dt;
        playerPosition.z += velocity.z * dt;

        const float worldRadius = static_cast<float>(kHalfTiles) * kTileSize - 0.8f;
        playerPosition.x = Clamp(playerPosition.x, -worldRadius, worldRadius);
        playerPosition.z = Clamp(playerPosition.z, -worldRadius, worldRadius);

        const float groundHeight = TerrainHeight(playerPosition.x, playerPosition.z);
        playerPosition.y = groundHeight + 0.74f;

        const float planarSpeed = Vector2Length({velocity.x, velocity.z});
        if (planarSpeed > 0.2f) playerYaw = std::atan2(velocity.z, velocity.x);

        if (sprint && planarSpeed > tuning.moveSpeed * 0.8f) moveState = MoveState::Run;
        else if (planarSpeed > 0.35f) moveState = MoveState::Walk;
        else moveState = MoveState::Idle;

        activeClip = ChooseClip(idleClip, walkClip, runClip, moveState);

        if (activeClip != nullptr && !activeClip->frames.empty()) {
            animTime += dt * activeClip->fps;
            const int frameCount = static_cast<int>(activeClip->frames.size());
            animFrameIndex = static_cast<int>(animTime) % frameCount;
        } else {
            animFrameIndex = 0;
        }

        if (sprint) stamina = Clamp(stamina - dt * 0.35f, 0.0f, 1.0f);
        else stamina = Clamp(stamina + dt * 0.2f, 0.0f, 1.0f);

        if (Vector3Length(moveWorld) > 0.01f) {
            faceLeft = Vector3DotProduct(moveWorld, camRight) > 0.0f;
        }

        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const Vector3 cameraOffset = {
            cp * std::cos(yaw) * tuning.cameraDistance,
            sp * tuning.cameraDistance,
            cp * std::sin(yaw) * tuning.cameraDistance
        };

        const Vector3 cameraTarget = {playerPosition.x, playerPosition.y + 0.6f, playerPosition.z};
        camera.target = Vector3Lerp(camera.target, cameraTarget, Clamp(dt * 8.0f, 0.0f, 1.0f));
        camera.position = Vector3Lerp(camera.position, Vector3Add(camera.target, cameraOffset), Clamp(dt * 8.0f, 0.0f, 1.0f));

        BeginDrawing();
        ClearBackground(Color{153, 210, 235, 255});

        BeginMode3D(camera);

        for (int x = -kHalfTiles; x <= kHalfTiles; ++x) {
            for (int z = -kHalfTiles; z <= kHalfTiles; ++z) {
                DrawWorldTile(static_cast<float>(x) * kTileSize, static_cast<float>(z) * kTileSize, kTileSize);
            }
        }

        for (const WorldProp& prop : props) DrawProp(prop);

        const float shadowSize = 0.45f;
        DrawCylinder({playerPosition.x, groundHeight + 0.01f, playerPosition.z}, shadowSize, shadowSize * 0.78f, 0.02f, 16, Fade(BLACK, 0.26f));

        if (activeClip != nullptr && !activeClip->frames.empty()) {
            const Texture2D& frame = activeClip->frames[animFrameIndex];
            Rectangle source = {0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};
            if (faceLeft) {
                source.x = static_cast<float>(frame.width);
                source.width = -static_cast<float>(frame.width);
            }

            const float spriteHeight = tuning.spriteHeight;
            const float spriteWidth = spriteHeight * (static_cast<float>(frame.width) / static_cast<float>(frame.height));
            const Vector3 spritePos = {playerPosition.x, groundHeight + spriteHeight * 0.5f + tuning.spriteFootOffset, playerPosition.z};
            DrawBillboardRec(camera, frame, source, spritePos, {spriteWidth, spriteHeight}, WHITE);
        } else {
            DrawCylinderEx(
                {playerPosition.x, groundHeight + 0.08f, playerPosition.z},
                {playerPosition.x, playerPosition.y + 0.55f, playerPosition.z},
                0.30f,
                0.24f,
                14,
                Color{236, 193, 89, 255});

            const Vector3 headPos = {playerPosition.x + std::cos(playerYaw) * 0.03f, playerPosition.y + 0.76f, playerPosition.z};
            DrawSphere(headPos, 0.22f, Color{244, 228, 167, 255});
        }

        if (tuning.drawGrid) DrawGrid(kHalfTiles * 2, 1.0f);

        EndMode3D();

        rlImGuiBegin();
        {
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(390, 0), ImGuiCond_Always);
            ImGui::Begin("Adventure Slice");
            ImGui::Text("Movement: WASD | Sprint: LSHIFT");
            ImGui::Separator();
            ImGui::Text("Clip: %s", activeClip ? activeClip->name.c_str() : "None");
            ImGui::Text("Frame: %d", animFrameIndex);
            ImGui::Text("Idle/Walk/Run: %d / %d / %d", static_cast<int>(idleClip.frames.size()), static_cast<int>(walkClip.frames.size()), static_cast<int>(runClip.frames.size()));
            if (idleClip.frames.empty() || walkClip.frames.empty() || runClip.frames.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Some animation frames are missing.");
            }
            if (ImGui::Button("Rebuild Props")) props = BuildProps();
            ImGui::Checkbox("Show Grid", &tuning.drawGrid);
            ImGui::Checkbox("Show Stats", &tuning.drawStats);
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(20, 250), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(390, 0), ImGuiCond_Always);
            ImGui::Begin("Tuning");
            ImGui::SliderFloat("Move Speed", &tuning.moveSpeed, 2.0f, 12.0f, "%.1f");
            ImGui::SliderFloat("Sprint Mult", &tuning.sprintMultiplier, 1.1f, 2.5f, "%.2f");
            ImGui::SliderFloat("Acceleration", &tuning.acceleration, 4.0f, 24.0f, "%.1f");
            ImGui::SliderFloat("Camera Dist", &tuning.cameraDistance, 12.0f, 32.0f, "%.1f");
            ImGui::SliderFloat("Camera Pitch", &tuning.cameraPitch, 24.0f, 60.0f, "%.1f");
            ImGui::SliderFloat("Camera Yaw", &tuning.cameraYaw, 30.0f, 60.0f, "%.1f");
            ImGui::SliderFloat("Sprite Height", &tuning.spriteHeight, 1.4f, 3.2f, "%.2f");
            ImGui::SliderFloat("Foot Offset", &tuning.spriteFootOffset, -0.3f, 0.3f, "%.2f");
            ImGui::End();

            if (tuning.drawStats) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth() - 300), 20), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_Always);
                ImGui::Begin("Runtime");
                ImGui::Text("FPS: %d", GetFPS());
                ImGui::Text("Player XZ: %.2f / %.2f", playerPosition.x, playerPosition.z);
                ImGui::Text("Elevation: %.2f", groundHeight);
                ImGui::Text("Props: %d", static_cast<int>(props.size()));
                ImGui::ProgressBar(stamina, ImVec2(-1.0f, 0.0f), sprint ? "Stamina (draining)" : "Stamina");
                ImGui::End();
            }
        }
        rlImGuiEnd();

        EndDrawing();
    }

    UnloadClip(idleClip);
    UnloadClip(walkClip);
    UnloadClip(runClip);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
