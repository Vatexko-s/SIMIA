#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "imgui.h"
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"

#include "src/AnimationSystem.h"
#include "src/CivilizationSystem.h"
#include "src/WorldSystem.h"

namespace {

constexpr int kScreenWidth = 1440;
constexpr int kScreenHeight = 900;
constexpr int kHalfTiles = 35;
constexpr float kTileSize = 1.2f;
constexpr float kTerrainBaseY = -0.55f;
constexpr int kInitialPopulation = 300;

struct GameTuning {
    float moveSpeed = 6.0f;
    float sprintMultiplier = 1.75f;
    float acceleration = 14.0f;
    static constexpr float cameraDistance = 37.0f;
    static constexpr float cameraPitch = 30.0f;
    static constexpr float cameraYaw = 45.0f;
    float spriteHeight = 2.15f;
    float spriteFootOffset = 0.03f;
    bool drawGrid = false;
    bool drawStats = true;
};

}  // namespace

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kScreenWidth, kScreenHeight, "SIMIA - Civilization Simulator");
    SetExitKey(KEY_NULL);
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
    std::vector<WorldProp> props = BuildProps(kHalfTiles, kTileSize, kTerrainBaseY);

    CivilizationSystem civilization(kTerrainBaseY, kHalfTiles, kTileSize);
    civilization.InitializePopulation(kInitialPopulation, idleClip, walkClip, runClip);

    Vector3 playerPosition = {0.0f, TerrainHeight(0.0f, 0.0f, kTerrainBaseY) + 0.75f, 0.0f};
    Vector3 velocity = {};
    float playerYaw = 0.0f;
    float stamina = 1.0f;

    MoveState moveState = MoveState::Idle;
    const AnimationClip* activeClip = &idleClip;
    float animTime = 0.0f;
    int animFrameIndex = 0;
    bool faceLeft = false;

    int hoveredAgentId = -1;
    int selectedAgentId = -1;
    Rectangle hoveredNpcRect = {};
    bool simulationPaused = false;

    while (!WindowShouldClose()) {
        const bool cmdQ = (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) && IsKeyPressed(KEY_Q);
        if (cmdQ) break;

        const float dt = GetFrameTime();
        if (!simulationPaused) {
            civilization.Update(dt, idleClip, walkClip, runClip);
        }

        Vector3 inputDir = {};
        if (IsKeyDown(KEY_W)) inputDir.z += 1.0f;
        if (IsKeyDown(KEY_S)) inputDir.z -= 1.0f;
        if (IsKeyDown(KEY_A)) inputDir.x -= 1.0f;
        if (IsKeyDown(KEY_D)) inputDir.x += 1.0f;
        if (Vector3Length(inputDir) > 0.0f) inputDir = Vector3Normalize(inputDir);

        const float yaw = DEG2RAD * GameTuning::cameraYaw;
        const float pitch = DEG2RAD * GameTuning::cameraPitch;

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

        const float groundHeight = TerrainHeight(playerPosition.x, playerPosition.z, kTerrainBaseY);
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
            cp * std::cos(yaw) * GameTuning::cameraDistance,
            sp * GameTuning::cameraDistance,
            cp * std::sin(yaw) * GameTuning::cameraDistance
        };

        const Vector3 cameraTarget = {playerPosition.x, playerPosition.y + 0.6f, playerPosition.z};
        camera.target = Vector3Lerp(camera.target, cameraTarget, Clamp(dt * 8.0f, 0.0f, 1.0f));
        camera.position = Vector3Lerp(camera.position, Vector3Add(camera.target, cameraOffset), Clamp(dt * 8.0f, 0.0f, 1.0f));

        BeginDrawing();
        ClearBackground(Color{153, 210, 235, 255});

        BeginMode3D(camera);

        for (int x = -kHalfTiles; x <= kHalfTiles; ++x) {
            for (int z = -kHalfTiles; z <= kHalfTiles; ++z) {
                DrawWorldTile(static_cast<float>(x) * kTileSize, static_cast<float>(z) * kTileSize, kTileSize, kTerrainBaseY);
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

        hoveredAgentId = -1;
        float hoveredNpcDepth = 1e30f;
        const Vector2 mousePos = GetMousePosition();

        const std::vector<Agent>& agents = civilization.Agents();
        for (const Agent& agent : agents) {
            if (!agent.alive || agent.activeClip == nullptr || agent.activeClip->frames.empty()) continue;

            const float npcGroundHeight = TerrainHeight(agent.position.x, agent.position.z, kTerrainBaseY);
            const Texture2D& frame = agent.activeClip->frames[agent.animFrameIndex];
            Rectangle source = {0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};

            Rectangle sourceCopy = source;
            if (agent.faceLeft) {
                sourceCopy.x = static_cast<float>(frame.width);
                sourceCopy.width = -static_cast<float>(frame.width);
            }

            const float spriteHeight = 1.7f;
            const float spriteWidth = spriteHeight * (static_cast<float>(frame.width) / static_cast<float>(frame.height));
            const Vector3 spritePos = {agent.position.x, npcGroundHeight + spriteHeight * 0.5f - 0.07f, agent.position.z};
            DrawBillboardRec(camera, frame, sourceCopy, spritePos, {spriteWidth, spriteHeight}, Fade(WHITE, 0.85f));

            const Vector3 topWorld = {spritePos.x, spritePos.y + spriteHeight * 0.5f, spritePos.z};
            const Vector3 bottomWorld = {spritePos.x, spritePos.y - spriteHeight * 0.5f, spritePos.z};
            const Vector2 topScreen = GetWorldToScreen(topWorld, camera);
            const Vector2 bottomScreen = GetWorldToScreen(bottomWorld, camera);
            const Vector2 centerScreen = GetWorldToScreen(spritePos, camera);
            const float pixelHeight = std::fabs(bottomScreen.y - topScreen.y);
            const float pixelWidth = pixelHeight * (static_cast<float>(frame.width) / static_cast<float>(frame.height));
            Rectangle screenRect = {
                centerScreen.x - pixelWidth * 0.5f,
                centerScreen.y - pixelHeight * 0.5f,
                pixelWidth,
                pixelHeight
            };

            if (CheckCollisionPointRec(mousePos, screenRect)) {
                const float dx = camera.position.x - agent.position.x;
                const float dy = camera.position.y - agent.position.y;
                const float dz = camera.position.z - agent.position.z;
                const float depth = dx * dx + dy * dy + dz * dz;
                if (depth < hoveredNpcDepth) {
                    hoveredNpcDepth = depth;
                    hoveredAgentId = agent.id;
                    hoveredNpcRect = screenRect;
                }
            }
        }

        if (tuning.drawGrid) DrawGrid(kHalfTiles * 2, 1.0f);

        EndMode3D();

        if (hoveredAgentId >= 0) {
            DrawRectangleLinesEx(hoveredNpcRect, 2.0f, WHITE);
        }

        rlImGuiBegin();
        {
            if (!ImGui::GetIO().WantCaptureMouse && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selectedAgentId = hoveredAgentId;
            }

            const CivilizationStats& simStats = civilization.Stats();
            const float yearsFloat = simStats.simTimeYears;
            const int yearNumber = static_cast<int>(yearsFloat) + 1;
            const float yearProgress = yearsFloat - std::floor(yearsFloat);

            ImGui::SetNextWindowPos(ImVec2(10, static_cast<float>(GetScreenHeight() - 94)), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(GetScreenWidth() - 20), 84), ImGuiCond_Always);
            ImGui::Begin("Timeline");
            ImGui::Text("Year: %d | Population: %d | Max Gen: %d", yearNumber, simStats.living, simStats.maxGeneration);
            ImGui::ProgressBar(yearProgress, ImVec2(-1.0f, 0.0f), "Year progress");

            if (simulationPaused) {
                if (ImGui::Button("Resume")) simulationPaused = false;
                ImGui::SameLine();
                if (ImGui::Button("Step +0.1s")) {
                    civilization.Update(0.1f, idleClip, walkClip, runClip);
                }
            } else {
                if (ImGui::Button("Pause")) simulationPaused = true;
            }
            ImGui::End();

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
            if (ImGui::Button("Rebuild Props")) props = BuildProps(kHalfTiles, kTileSize, kTerrainBaseY);
            ImGui::Checkbox("Show Grid", &tuning.drawGrid);
            ImGui::Checkbox("Show Stats", &tuning.drawStats);
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(20, 250), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(390, 0), ImGuiCond_Always);
            ImGui::Begin("Tuning");
            ImGui::SliderFloat("Move Speed", &tuning.moveSpeed, 2.0f, 12.0f, "%.1f");
            ImGui::SliderFloat("Sprint Mult", &tuning.sprintMultiplier, 1.1f, 2.5f, "%.2f");
            ImGui::SliderFloat("Acceleration", &tuning.acceleration, 4.0f, 24.0f, "%.1f");
            ImGui::SliderFloat("Sprite Height", &tuning.spriteHeight, 1.4f, 3.2f, "%.2f");
            ImGui::SliderFloat("Foot Offset", &tuning.spriteFootOffset, -0.3f, 0.3f, "%.2f");
            ImGui::End();

            if (tuning.drawStats) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth() - 320), 20), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
                ImGui::Begin("Runtime");
                ImGui::Text("FPS: %d", GetFPS());
                ImGui::Text("Player XZ: %.2f / %.2f", playerPosition.x, playerPosition.z);
                ImGui::Text("Elevation: %.2f", groundHeight);
                ImGui::Text("Population: %d", simStats.living);
                ImGui::Text("Born / Dead: %d / %d", simStats.totalBorn, simStats.totalDead);
                ImGui::Text("Year B/D: %d / %d", simStats.birthsThisEpoch, simStats.deathsThisEpoch);
                ImGui::Text("Last Year B/D: %d / %d", simStats.lastEpochBirths, simStats.lastEpochDeaths);
                ImGui::Text("Max Gen: %d", simStats.maxGeneration);
                ImGui::Text("Year(Epoch): %d | Tick: %d", simStats.epoch + 1, simStats.tick);
                ImGui::Text("State: %s", simulationPaused ? "Paused" : "Running");
                ImGui::Text("Fertile F/M: %d / %d", simStats.fertileFemales, simStats.fertileMales);
                ImGui::Text("Repro checks/success: %d / %d", simStats.reproductionChecks, simStats.reproductionSuccesses);
                ImGui::Text("Pressure: %.2f", simStats.populationPressure);
                ImGui::Text("Sim Years: %.1f", simStats.simTimeYears);
                ImGui::ProgressBar(stamina, ImVec2(-1.0f, 0.0f), sprint ? "Stamina (draining)" : "Stamina");
                ImGui::End();
            }

            const Agent* selectedAgent = nullptr;
            for (const Agent& agent : civilization.Agents()) {
                if (agent.id == selectedAgentId) {
                    selectedAgent = &agent;
                    break;
                }
            }
            if (selectedAgent == nullptr) selectedAgentId = -1;

            if (selectedAgent != nullptr) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth() - 320), 290), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
                ImGui::Begin("NPC Stats");
                ImGui::Text("ID: %d", selectedAgent->id);
                ImGui::Text("Generation: %d", selectedAgent->generation);
                ImGui::Text("Sex: %s", selectedAgent->sex == Sex::Female ? "Female" : "Male");
                ImGui::Text("State: %s", MoveStateLabel(selectedAgent->moveState));
                ImGui::Text("Age: %.1f / %.1f years", selectedAgent->age, selectedAgent->maxAge);
                ImGui::Text("Energy: %.2f", selectedAgent->energy);
                ImGui::Text("Cooldown: %.2f", selectedAgent->fertilityCooldown);
                ImGui::Separator();
                ImGui::Text("Genome Move: %.2f", selectedAgent->genome.moveSpeed);
                ImGui::Text("Genome Metab: %.2f", selectedAgent->genome.metabolism);
                ImGui::Text("Genome Fert: %.2f", selectedAgent->genome.fertility);
                ImGui::Text("Genome Long: %.2f", selectedAgent->genome.longevity);
                ImGui::Separator();
                ImGui::Text("Position: %.2f / %.2f / %.2f", selectedAgent->position.x, selectedAgent->position.y, selectedAgent->position.z);
                ImGui::Text("Velocity: %.2f / %.2f / %.2f", selectedAgent->velocity.x, selectedAgent->velocity.y, selectedAgent->velocity.z);
                ImGui::Text("Clip: %s", selectedAgent->activeClip ? selectedAgent->activeClip->name.c_str() : "None");
                ImGui::Text("Frame: %d", selectedAgent->animFrameIndex);
                if (ImGui::Button("Close NPC Stats")) {
                    selectedAgentId = -1;
                }
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
