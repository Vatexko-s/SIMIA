#include <algorithm>
#include <cmath>
#include <cstdio>
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
constexpr int kHalfTiles = 60;
constexpr float kTileSize = 1.2f;
constexpr float kTerrainBaseY = -0.55f;
constexpr int kInitialPopulation = 350;
constexpr float kInteractRange = 2.4f;

struct GameTuning {
    float moveSpeed = 6.0f;
    float sprintMultiplier = 1.85f;
    float acceleration = 14.0f;
    static constexpr float cameraDistance = 44.0f;
    static constexpr float cameraPitch = 30.0f;
    static constexpr float cameraYaw = 45.0f;
    float spriteHeight = 2.15f;
    float spriteFootOffset = 0.03f;
    bool drawGrid = false;
    bool drawStats = true;
    bool showInteractHint = true;
};

const char* BehaviorLabel(Behavior b) {
    switch (b) {
        case Behavior::Wander: return "Wander";
        case Behavior::SeekFood: return "Seek Food";
        case Behavior::Eat: return "Eating";
        case Behavior::SeekWater: return "Seek Water";
        case Behavior::Drink: return "Drinking";
        case Behavior::SeekMate: return "Seek Mate";
        case Behavior::Rest: return "Resting";
    }
    return "?";
}

const char* DeathCauseLabel(DeathCause c) {
    switch (c) {
        case DeathCause::None: return "Alive";
        case DeathCause::Age: return "Age";
        case DeathCause::Starvation: return "Starvation";
    }
    return "?";
}

void DrawGeneBar(const char* label, float value, float minVal, float maxVal, ImU32 color) {
    const float t = Clamp((value - minVal) / (maxVal - minVal), 0.0f, 1.0f);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(120);
    ImGui::Text("%.2f", value);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 12.0f;
    const ImVec2 a = cursor;
    const ImVec2 b = ImVec2(cursor.x + width, cursor.y + height);
    dl->AddRectFilled(a, b, IM_COL32(40, 40, 50, 200), 4.0f);
    const ImVec2 fillEnd = ImVec2(cursor.x + width * t, cursor.y + height);
    dl->AddRectFilled(a, fillEnd, color, 4.0f);
    dl->AddRect(a, b, IM_COL32(255, 255, 255, 80), 4.0f);
    ImGui::Dummy(ImVec2(width, height + 4.0f));
}

void DrawDnaHelix(float regionWidth, float regionHeight, const Genome& g) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    const float w = regionWidth;
    const float h = regionHeight;
    const float centerX = origin.x + w * 0.5f;
    const int segments = 64;
    const float helixRadius = w * 0.24f;
    const float twists = 2.4f;

    const float values[6] = {
        g.moveSpeed, g.metabolism, g.fertility, g.longevity, g.vision, g.strength
    };
    const ImU32 colors[6] = {
        IM_COL32(120, 200, 255, 255),
        IM_COL32(255, 140, 90, 255),
        IM_COL32(255, 110, 170, 255),
        IM_COL32(180, 230, 130, 255),
        IM_COL32(255, 215, 100, 255),
        IM_COL32(195, 130, 240, 255),
    };

    auto strandX = [&](int i, float side) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = t * twists * 2.0f * PI;
        return centerX + std::sin(angle) * helixRadius * side;
    };
    auto strandY = [&](int i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        return origin.y + 8.0f + t * (h - 16.0f);
    };

    for (int i = 0; i < segments; ++i) {
        const ImVec2 p0L = ImVec2(strandX(i, 1.0f), strandY(i));
        const ImVec2 p1L = ImVec2(strandX(i + 1, 1.0f), strandY(i + 1));
        const ImVec2 p0R = ImVec2(strandX(i, -1.0f), strandY(i));
        const ImVec2 p1R = ImVec2(strandX(i + 1, -1.0f), strandY(i + 1));
        dl->AddLine(p0L, p1L, IM_COL32(220, 220, 240, 230), 2.5f);
        dl->AddLine(p0R, p1R, IM_COL32(220, 220, 240, 230), 2.5f);
    }

    const int rungCount = 6;
    for (int r = 0; r < rungCount; ++r) {
        const int i = static_cast<int>((static_cast<float>(r) + 0.5f) * (segments / static_cast<float>(rungCount)));
        const ImVec2 left = ImVec2(strandX(i, 1.0f), strandY(i));
        const ImVec2 right = ImVec2(strandX(i, -1.0f), strandY(i));
        const float val = values[r];
        const float intensity = Clamp((val - 0.5f) / 1.1f, 0.0f, 1.0f);
        const ImU32 col = colors[r];
        const ImU32 alpha = static_cast<ImU32>(80 + intensity * 175);
        const ImU32 dim = (col & 0x00FFFFFF) | (alpha << 24);
        dl->AddLine(left, right, dim, 2.0f + intensity * 2.0f);
        dl->AddCircleFilled(left, 3.0f + intensity * 1.5f, col);
        dl->AddCircleFilled(right, 3.0f + intensity * 1.5f, col);
    }

    ImGui::Dummy(ImVec2(w, h));
}

}  // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(kScreenWidth, kScreenHeight, "SIMIA - Civilization Simulator");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

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
    unsigned int worldSeed = static_cast<unsigned int>(GetTime() * 1000.0) ^ 0xDEADBEEFu;
    std::vector<WorldProp> props = BuildProps(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);
    std::vector<FoodNode> foodNodes = BuildFoodNodes(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);

    CivilizationSystem civilization(kTerrainBaseY, kHalfTiles, kTileSize);
    civilization.InitializePopulation(kInitialPopulation, idleClip, walkClip, runClip);

    RenderTexture2D dnaTexture = LoadRenderTexture(360, 420);
    SetTextureFilter(dnaTexture.texture, TEXTURE_FILTER_BILINEAR);
    float dnaSpinAngle = 0.0f;

    Vector3 playerPosition = {0.0f, TerrainHeight(0.0f, 0.0f, kTerrainBaseY) + 0.75f, 0.0f};
    Vector3 velocity = {};
    float playerYaw = 0.0f;

    MoveState moveState = MoveState::Idle;
    const AnimationClip* activeClip = &idleClip;
    float animTime = 0.0f;
    int animFrameIndex = 0;
    bool faceLeft = false;

    int nearestAgentId = -1;
    int selectedAgentId = -1;
    bool simulationPaused = false;
    bool showDnaWindow = false;
    float simSpeed = 1.0f;

    constexpr float kRenderRadius = 55.0f;
    constexpr float kRenderRadiusSq = kRenderRadius * kRenderRadius;
    constexpr float kAgentRenderRadius = 60.0f;
    constexpr float kAgentRenderRadiusSq = kAgentRenderRadius * kAgentRenderRadius;

    while (!WindowShouldClose()) {
        const bool cmdQ = (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) && IsKeyPressed(KEY_Q);
        if (cmdQ) break;

        const float dt = GetFrameTime();
        if (!simulationPaused) {
            const float simDt = dt * simSpeed;
            civilization.Update(simDt, foodNodes, idleClip, walkClip, runClip);
            UpdateFoodNodes(foodNodes, simDt);
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

        const bool sprint = IsKeyDown(KEY_LEFT_SHIFT);
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

        if (Vector3Length(moveWorld) > 0.01f) {
            faceLeft = Vector3DotProduct(moveWorld, camRight) > 0.0f;
        }

        nearestAgentId = civilization.FindAgentNearestTo(playerPosition, kInteractRange);

        if (!ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(KEY_E)) {
            if (nearestAgentId >= 0) {
                if (selectedAgentId == nearestAgentId) {
                    selectedAgentId = -1;
                    showDnaWindow = false;
                } else {
                    selectedAgentId = nearestAgentId;
                    showDnaWindow = false;
                }
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(KEY_ESCAPE)) {
            selectedAgentId = -1;
            showDnaWindow = false;
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

        const int tileRadius = static_cast<int>(kRenderRadius / kTileSize) + 1;
        const int playerTileX = static_cast<int>(std::round(playerPosition.x / kTileSize));
        const int playerTileZ = static_cast<int>(std::round(playerPosition.z / kTileSize));
        const int xMin = std::max(-kHalfTiles, playerTileX - tileRadius);
        const int xMax = std::min(kHalfTiles, playerTileX + tileRadius);
        const int zMin = std::max(-kHalfTiles, playerTileZ - tileRadius);
        const int zMax = std::min(kHalfTiles, playerTileZ + tileRadius);
        for (int x = xMin; x <= xMax; ++x) {
            const float wx = static_cast<float>(x) * kTileSize;
            const float dx = wx - playerPosition.x;
            for (int z = zMin; z <= zMax; ++z) {
                const float wz = static_cast<float>(z) * kTileSize;
                const float dz = wz - playerPosition.z;
                if (dx * dx + dz * dz > kRenderRadiusSq) continue;
                DrawWorldTile(wx, wz, kTileSize, kTerrainBaseY);
            }
        }

        for (const WorldProp& prop : props) {
            const float dx = prop.position.x - playerPosition.x;
            const float dz = prop.position.z - playerPosition.z;
            if (dx * dx + dz * dz > kRenderRadiusSq) continue;
            DrawProp(prop);
        }
        for (const FoodNode& node : foodNodes) {
            const float dx = node.position.x - playerPosition.x;
            const float dz = node.position.z - playerPosition.z;
            if (dx * dx + dz * dz > kRenderRadiusSq) continue;
            DrawFoodNode(node);
        }

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

        const std::vector<Agent>& agents = civilization.Agents();

        DrawCircle3D({playerPosition.x, groundHeight + 0.04f, playerPosition.z}, kInteractRange,
                     {1.0f, 0.0f, 0.0f}, 90.0f, Fade(WHITE, 0.18f));

        Vector2 nearestAgentScreen = {0.0f, 0.0f};
        bool nearestAgentScreenValid = false;

        for (const Agent& agent : agents) {
            if (!agent.alive || agent.activeClip == nullptr || agent.activeClip->frames.empty()) continue;

            const float adx = agent.position.x - playerPosition.x;
            const float adz = agent.position.z - playerPosition.z;
            const bool isHighlighted = (agent.id == nearestAgentId || agent.id == selectedAgentId);
            if (!isHighlighted && adx * adx + adz * adz > kAgentRenderRadiusSq) continue;

            const float npcGroundHeight = TerrainHeight(agent.position.x, agent.position.z, kTerrainBaseY);
            const Texture2D& frame = agent.activeClip->frames[agent.animFrameIndex];
            Rectangle source = {0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};

            if (agent.faceLeft) {
                source.x = static_cast<float>(frame.width);
                source.width = -static_cast<float>(frame.width);
            }

            const float spriteHeight = 1.7f;
            const float spriteWidth = spriteHeight * (static_cast<float>(frame.width) / static_cast<float>(frame.height));
            const Vector3 spritePos = {agent.position.x, npcGroundHeight + spriteHeight * 0.5f - 0.07f, agent.position.z};
            const Color tint = (agent.id == selectedAgentId)
                ? Color{255, 220, 120, 255}
                : ((agent.id == nearestAgentId) ? Color{200, 240, 255, 255} : WHITE);
            DrawBillboardRec(camera, frame, source, spritePos, {spriteWidth, spriteHeight}, Fade(tint, 0.92f));

            if (agent.id == nearestAgentId || agent.id == selectedAgentId) {
                const Color ring = (agent.id == selectedAgentId) ? Color{255, 200, 90, 255} : Color{120, 220, 255, 255};
                DrawCircle3D({agent.position.x, npcGroundHeight + 0.04f, agent.position.z}, 0.7f,
                             {1.0f, 0.0f, 0.0f}, 90.0f, ring);
                if (agent.id == nearestAgentId) {
                    const Vector3 headLabel = {spritePos.x, spritePos.y + spriteHeight * 0.55f, spritePos.z};
                    nearestAgentScreen = GetWorldToScreen(headLabel, camera);
                    nearestAgentScreenValid = true;
                }
            }
        }

        if (tuning.drawGrid) DrawGrid(kHalfTiles * 2, 1.0f);

        EndMode3D();

        if (nearestAgentScreenValid && tuning.showInteractHint && nearestAgentId != selectedAgentId) {
            const char* msg = "Press [E] to inspect";
            const int fontSize = 18;
            const int textW = MeasureText(msg, fontSize);
            const int padX = 10;
            const int padY = 6;
            const Rectangle box = {
                nearestAgentScreen.x - textW * 0.5f - padX,
                nearestAgentScreen.y - 38.0f,
                static_cast<float>(textW + padX * 2),
                static_cast<float>(fontSize + padY * 2)
            };
            DrawRectangleRounded(box, 0.4f, 6, Fade(BLACK, 0.65f));
            DrawRectangleRoundedLines(box, 0.4f, 6, 1.5f, Fade(WHITE, 0.55f));
            DrawText(msg, static_cast<int>(box.x + padX), static_cast<int>(box.y + padY), fontSize, WHITE);
        }

        rlImGuiBegin();
        {
            const CivilizationStats& simStats = civilization.Stats();
            const float yearsFloat = simStats.simTimeYears;
            const int yearNumber = static_cast<int>(yearsFloat) + 1;
            const float yearProgress = yearsFloat - std::floor(yearsFloat);

            ImGui::SetNextWindowPos(ImVec2(10, static_cast<float>(GetScreenHeight() - 124)), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(GetScreenWidth() - 20), 114), ImGuiCond_Always);
            ImGui::Begin("Timeline");
            ImGui::Text("Year: %d | Pop: %d | MaxGen: %d | Forage: %d | Eat: %d | Drink: %d | Speed: %.2fx",
                        yearNumber, simStats.living, simStats.maxGeneration,
                        simStats.foragingAgents, simStats.eatingAgents, simStats.drinkingAgents, simSpeed);
            ImGui::ProgressBar(yearProgress, ImVec2(-1.0f, 0.0f), "Year progress");

            if (simulationPaused) {
                if (ImGui::Button("Resume")) simulationPaused = false;
                ImGui::SameLine();
                if (ImGui::Button("Step +0.1s")) {
                    civilization.Update(0.1f, foodNodes, idleClip, walkClip, runClip);
                    UpdateFoodNodes(foodNodes, 0.1f);
                }
            } else {
                if (ImGui::Button("Pause")) simulationPaused = true;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(" Speed:");
            ImGui::SameLine();
            const float speedPresets[6] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
            const char* speedLabels[6] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
            for (int s = 0; s < 6; ++s) {
                const bool active = std::fabs(simSpeed - speedPresets[s]) < 0.01f;
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.65f, 0.20f, 1.0f));
                if (ImGui::Button(speedLabels[s])) simSpeed = speedPresets[s];
                if (active) ImGui::PopStyleColor();
                ImGui::SameLine();
            }
            ImGui::SetNextItemWidth(140.0f);
            ImGui::SliderFloat("##speed", &simSpeed, 0.0f, 10.0f, "%.2fx");
            ImGui::SameLine();
            if (ImGui::Button("Respawn World")) {
                worldSeed = static_cast<unsigned int>(GetTime() * 1000.0) ^ 0xDEADBEEFu;
                foodNodes = BuildFoodNodes(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);
                props = BuildProps(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);
                civilization.InitializePopulation(kInitialPopulation, idleClip, walkClip, runClip);
                selectedAgentId = -1;
            }
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(390, 0), ImGuiCond_Always);
            ImGui::Begin("Adventure Slice");
            ImGui::Text("Move: WASD  |  Sprint: LSHIFT");
            ImGui::Text("Approach a monkey + press [E] to inspect");
            ImGui::Text("[ESC] close inspector");
            ImGui::Separator();
            ImGui::Text("Clip: %s", activeClip ? activeClip->name.c_str() : "None");
            ImGui::Text("Frame: %d", animFrameIndex);
            ImGui::Text("Idle/Walk/Run: %d / %d / %d",
                        static_cast<int>(idleClip.frames.size()),
                        static_cast<int>(walkClip.frames.size()),
                        static_cast<int>(runClip.frames.size()));
            if (idleClip.frames.empty() || walkClip.frames.empty() || runClip.frames.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Some animation frames are missing.");
            }
            if (ImGui::Button("Rebuild Props")) {
                worldSeed = static_cast<unsigned int>(GetTime() * 1000.0) ^ 0xC0FFEE11u;
                props = BuildProps(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);
            }
            ImGui::SameLine();
            if (ImGui::Button("Rebuild Food")) {
                worldSeed = static_cast<unsigned int>(GetTime() * 1000.0) ^ 0xBADF00Du;
                foodNodes = BuildFoodNodes(kHalfTiles, kTileSize, kTerrainBaseY, worldSeed);
            }
            ImGui::Checkbox("Show Grid", &tuning.drawGrid);
            ImGui::Checkbox("Show Stats", &tuning.drawStats);
            ImGui::Checkbox("Show Interact Hint", &tuning.showInteractHint);
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
                ImGui::Separator();
                ImGui::Text("Population: %d", simStats.living);
                ImGui::Text("Born / Dead: %d / %d", simStats.totalBorn, simStats.totalDead);
                ImGui::Text("Year B/D: %d / %d", simStats.birthsThisEpoch, simStats.deathsThisEpoch);
                ImGui::Text("Last Year B/D: %d / %d", simStats.lastEpochBirths, simStats.lastEpochDeaths);
                ImGui::Text("Max Gen: %d", simStats.maxGeneration);
                ImGui::Text("Year(Epoch): %d | Tick: %d", simStats.epoch + 1, simStats.tick);
                ImGui::Text("State: %s", simulationPaused ? "Paused" : "Running");
                ImGui::Separator();
                ImGui::Text("Fertile F/M: %d / %d", simStats.fertileFemales, simStats.fertileMales);
                ImGui::Text("Repro check/success: %d / %d", simStats.reproductionChecks, simStats.reproductionSuccesses);
                ImGui::Text("Pressure: %.2f", simStats.populationPressure);
                ImGui::Text("Forage / Eat / Drink: %d / %d / %d",
                            simStats.foragingAgents, simStats.eatingAgents, simStats.drinkingAgents);
                ImGui::Text("Deaths Age / Starve: %d / %d", simStats.ageDeaths, simStats.starvationDeaths);
                ImGui::Text("Mating seekers: %d", simStats.matingSeekers);
                ImGui::Text("Sim Years: %.1f", simStats.simTimeYears);
                ImGui::Text("Sprint: %s", sprint ? "ON" : "off");
                ImGui::End();
            }

            const Agent* selectedAgent = (selectedAgentId >= 0) ? civilization.FindAgentById(selectedAgentId) : nullptr;
            if (selectedAgent != nullptr && !selectedAgent->alive) selectedAgent = nullptr;
            if (selectedAgent == nullptr) {
                selectedAgentId = -1;
                showDnaWindow = false;
            }

            if (selectedAgent != nullptr) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth() - 360), 290), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
                ImGui::Begin("Monkey Inspector");
                ImGui::Text("ID #%d  |  Gen %d  |  %s",
                            selectedAgent->id, selectedAgent->generation,
                            selectedAgent->sex == Sex::Female ? "Female" : "Male");
                ImGui::Text("Behavior: %s", BehaviorLabel(selectedAgent->behavior));
                ImGui::Text("Move: %s", MoveStateLabel(selectedAgent->moveState));
                ImGui::Text("Status: %s", DeathCauseLabel(selectedAgent->deathCause));
                ImGui::Separator();

                const float ageRatio = Clamp(selectedAgent->age / std::max(selectedAgent->maxAge, 0.0001f), 0.0f, 1.0f);
                char ageLabel[64];
                std::snprintf(ageLabel, sizeof(ageLabel), "Age %.1f / %.1f y", selectedAgent->age, selectedAgent->maxAge);
                ImGui::ProgressBar(ageRatio, ImVec2(-1.0f, 0.0f), ageLabel);

                char eLabel[32];
                std::snprintf(eLabel, sizeof(eLabel), "Energy %.0f%%", selectedAgent->energy * 100.0f);
                ImGui::ProgressBar(selectedAgent->energy, ImVec2(-1.0f, 0.0f), eLabel);

                char hLabel[32];
                std::snprintf(hLabel, sizeof(hLabel), "Hydration %.0f%%", selectedAgent->hydration * 100.0f);
                ImGui::ProgressBar(selectedAgent->hydration, ImVec2(-1.0f, 0.0f), hLabel);

                ImGui::Text("Children born: %d", selectedAgent->childrenBorn);
                ImGui::Text("Fertility cooldown: %.2f y", selectedAgent->fertilityCooldown);
                ImGui::Separator();
                ImGui::Text("Position: %.1f / %.1f / %.1f", selectedAgent->position.x, selectedAgent->position.y, selectedAgent->position.z);
                ImGui::Text("Velocity: %.2f / %.2f", selectedAgent->velocity.x, selectedAgent->velocity.z);
                ImGui::Separator();

                if (ImGui::Button(showDnaWindow ? "Hide DNA" : "View DNA")) {
                    showDnaWindow = !showDnaWindow;
                }
                ImGui::SameLine();
                if (ImGui::Button("Close")) {
                    selectedAgentId = -1;
                    showDnaWindow = false;
                }
                ImGui::End();
            }

            if (selectedAgent != nullptr && showDnaWindow) {
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth() - 760), 290), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(390, 520), ImGuiCond_Always);
                ImGui::Begin("DNA / Genome");
                ImGui::Text("Specimen #%d - Generation %d", selectedAgent->id, selectedAgent->generation);
                ImGui::Separator();

                const float helixHeight = 230.0f;
                const float helixWidth = ImGui::GetContentRegionAvail().x;
                DrawDnaHelix(helixWidth, helixHeight, selectedAgent->genome);

                ImGui::Separator();
                ImGui::TextUnformatted("Gene Expression");
                ImGui::Spacing();

                DrawGeneBar("Move Speed", selectedAgent->genome.moveSpeed, 0.5f, 1.6f, IM_COL32(120, 200, 255, 230));
                DrawGeneBar("Metabolism", selectedAgent->genome.metabolism, 0.5f, 1.6f, IM_COL32(255, 140, 90, 230));
                DrawGeneBar("Fertility", selectedAgent->genome.fertility, 0.5f, 1.6f, IM_COL32(255, 110, 170, 230));
                DrawGeneBar("Longevity", selectedAgent->genome.longevity, 0.5f, 1.6f, IM_COL32(180, 230, 130, 230));
                DrawGeneBar("Vision", selectedAgent->genome.vision, 0.5f, 1.6f, IM_COL32(255, 215, 100, 230));
                DrawGeneBar("Strength", selectedAgent->genome.strength, 0.5f, 1.6f, IM_COL32(195, 130, 240, 230));

                ImGui::Separator();
                const float fitness =
                    selectedAgent->genome.moveSpeed * 0.18f +
                    (2.0f - selectedAgent->genome.metabolism) * 0.18f +
                    selectedAgent->genome.fertility * 0.18f +
                    selectedAgent->genome.longevity * 0.18f +
                    selectedAgent->genome.vision * 0.14f +
                    selectedAgent->genome.strength * 0.14f;
                ImGui::Text("Composite fitness index: %.2f", fitness);
                ImGui::TextWrapped("Helix rungs are color-coded per gene; rung thickness/brightness scale with expression.");
                ImGui::End();
            }

            if (nearestAgentId >= 0 && selectedAgentId != nearestAgentId) {
                ImGui::SetNextWindowPos(ImVec2(20, static_cast<float>(GetScreenHeight() - 170)), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_Always);
                ImGui::Begin("##InteractHint", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Monkey #%d in range", nearestAgentId);
                ImGui::Text("Press [E] to inspect");
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
