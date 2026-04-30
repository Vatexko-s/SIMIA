#include "WorldSystem.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace {
float gTerrainOffsetX = 0.0f;
float gTerrainOffsetZ = 0.0f;
float gTerrainPhaseA = 0.0f;
float gTerrainPhaseB = 0.0f;
float gTerrainFreqJitter = 1.0f;
}

void SetTerrainSeed(unsigned int seed) {
    SetRandomSeed(seed ^ 0xDEFACE00u);
    gTerrainOffsetX = static_cast<float>(GetRandomValue(-100000, 100000)) * 0.01f;
    gTerrainOffsetZ = static_cast<float>(GetRandomValue(-100000, 100000)) * 0.01f;
    gTerrainPhaseA = static_cast<float>(GetRandomValue(0, 6283)) * 0.001f;
    gTerrainPhaseB = static_cast<float>(GetRandomValue(0, 6283)) * 0.001f;
    gTerrainFreqJitter = 0.85f + static_cast<float>(GetRandomValue(0, 1000)) * 0.0003f;
}

float TerrainHeight(float x, float z, float terrainBaseY) {
    const float xx = x + gTerrainOffsetX;
    const float zz = z + gTerrainOffsetZ;
    const float fA = 0.22f * gTerrainFreqJitter;
    const float fB = 0.19f * gTerrainFreqJitter;
    const float fC = 0.32f * gTerrainFreqJitter;
    const float fD = 0.27f * gTerrainFreqJitter;
    const float waves = std::sin(xx * fA + gTerrainPhaseA) * 0.45f + std::cos(zz * fB + gTerrainPhaseB) * 0.35f;
    const float details = std::sin((xx + zz) * fC + gTerrainPhaseB) * 0.22f + std::cos((xx - zz) * fD + gTerrainPhaseA) * 0.14f;
    return terrainBaseY + waves + details;
}

bool IsWaterAt(float x, float z, float terrainBaseY) {
    return TerrainHeight(x, z, terrainBaseY) <= -0.38f;
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

void DrawWorldTile(float x, float z, float size, float terrainBaseY, Color nationalityTint) {
    const float h = TerrainHeight(x, z, terrainBaseY);
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

    if (nationalityTint.a > 0 && h > waterLevel) {
        top = LerpColor(top, nationalityTint, 0.18f);
    }

    const float thickness = h - terrainBaseY + 0.35f;
    const Vector3 tilePos = {x, terrainBaseY + thickness * 0.5f, z};
    DrawCubeV(tilePos, {size, thickness, size}, top);

    if (h <= waterLevel + 0.03f) {
        DrawCubeV({x, waterLevel + 0.02f, z}, {size * 0.94f, 0.04f, size * 0.94f}, Fade(Color{95, 182, 220, 255}, 0.85f));
    }
}

std::vector<Nationality> BuildNationalities(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed) {
    SetRandomSeed(seed ^ 0xCAFEBABEu);

    const Color colors[4] = {
        Color{120, 170, 255, 255},   // Azura - blue
        Color{255, 230, 120, 255},   // Solana - yellow
        Color{255, 165, 90, 255},    // Embra - orange
        Color{130, 220, 140, 255},   // Verdia - green
    };
    const char* names[4] = {"Azura", "Solana", "Embra", "Verdia"};

    std::vector<Nationality> nations;
    nations.reserve(4);

    for (int i = 0; i < 4; ++i) {
        Nationality n;
        n.name = names[i];
        n.color = colors[i];

        Vector3 pos = {0.0f, 0.0f, 0.0f};
        int attempts = 0;
        const int range = halfTiles - 5;
        do {
            pos.x = static_cast<float>(GetRandomValue(-range, range)) * tileSize;
            pos.z = static_cast<float>(GetRandomValue(-range, range)) * tileSize;
            ++attempts;
        } while (IsWaterAt(pos.x, pos.z, terrainBaseY) && attempts < 400);
        pos.y = TerrainHeight(pos.x, pos.z, terrainBaseY);

        // Spread seeds across quadrants by biasing first attempt direction.
        const float quadAngle = (static_cast<float>(i) + 0.5f) * (2.0f * PI / 4.0f);
        if (attempts >= 400) {
            pos.x = std::cos(quadAngle) * static_cast<float>(range) * tileSize * 0.6f;
            pos.z = std::sin(quadAngle) * static_cast<float>(range) * tileSize * 0.6f;
            pos.y = TerrainHeight(pos.x, pos.z, terrainBaseY);
        }

        n.seed = pos;
        nations.push_back(n);
    }

    return nations;
}

int NationalityIdAt(float x, float z, const std::vector<Nationality>& nations, float terrainBaseY) {
    if (IsWaterAt(x, z, terrainBaseY)) return -1;
    int best = -1;
    float bestD = 1e18f;
    for (int i = 0; i < static_cast<int>(nations.size()); ++i) {
        const float dx = nations[i].seed.x - x;
        const float dz = nations[i].seed.z - z;
        const float d = dx * dx + dz * dz;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

std::vector<WorldProp> BuildProps(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed) {
    std::vector<WorldProp> props;

    // Density scales with world area; tree-heavy ratio.
    const int target = static_cast<int>(halfTiles * halfTiles * 0.55f);
    props.reserve(target);

    SetRandomSeed(seed);
    for (int i = 0; i < target; ++i) {
        const float x = static_cast<float>(GetRandomValue(-halfTiles + 1, halfTiles - 1)) * tileSize +
                        static_cast<float>(GetRandomValue(-45, 45)) * 0.01f;
        const float z = static_cast<float>(GetRandomValue(-halfTiles + 1, halfTiles - 1)) * tileSize +
                        static_cast<float>(GetRandomValue(-45, 45)) * 0.01f;

        if (std::fabs(x - z * 0.25f) < 1.4f) continue;
        if (TerrainHeight(x, z, terrainBaseY) < -0.25f) continue;

        // 80 % trees so forests dominate.
        const bool tree = GetRandomValue(0, 100) > 20;
        const float scale = tree ? static_cast<float>(GetRandomValue(80, 150)) * 0.01f
                                 : static_cast<float>(GetRandomValue(45, 110)) * 0.01f;
        props.push_back({{x, TerrainHeight(x, z, terrainBaseY), z}, scale, tree});
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

std::vector<FoodNode> BuildFoodNodes(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed) {
    std::vector<FoodNode> nodes;
    const int berryTarget = static_cast<int>(halfTiles * halfTiles * 0.18f);
    const int treeTarget = static_cast<int>(halfTiles * halfTiles * 0.06f);
    nodes.reserve(berryTarget + treeTarget + 100);

    SetRandomSeed(seed ^ 0x9E3779B9u);

    // Berry bushes scattered across grass.
    for (int i = 0; i < berryTarget; ++i) {
        const float x = static_cast<float>(GetRandomValue(-halfTiles + 2, halfTiles - 2)) * tileSize +
                        static_cast<float>(GetRandomValue(-40, 40)) * 0.01f;
        const float z = static_cast<float>(GetRandomValue(-halfTiles + 2, halfTiles - 2)) * tileSize +
                        static_cast<float>(GetRandomValue(-40, 40)) * 0.01f;
        if (TerrainHeight(x, z, terrainBaseY) < -0.20f) continue;
        if (std::fabs(x - z * 0.25f) < 1.5f) continue;

        FoodNode node;
        node.kind = FoodKind::BerryBush;
        node.position = {x, TerrainHeight(x, z, terrainBaseY), z};
        node.maxAmount = static_cast<float>(GetRandomValue(60, 120)) * 0.01f;
        node.amount = node.maxAmount;
        node.regrowRate = static_cast<float>(GetRandomValue(2, 6)) * 0.01f;
        nodes.push_back(node);
    }

    // Fruit trees - bigger, slower regrow.
    for (int i = 0; i < treeTarget; ++i) {
        const float x = static_cast<float>(GetRandomValue(-halfTiles + 3, halfTiles - 3)) * tileSize +
                        static_cast<float>(GetRandomValue(-30, 30)) * 0.01f;
        const float z = static_cast<float>(GetRandomValue(-halfTiles + 3, halfTiles - 3)) * tileSize +
                        static_cast<float>(GetRandomValue(-30, 30)) * 0.01f;
        if (TerrainHeight(x, z, terrainBaseY) < -0.10f) continue;

        FoodNode node;
        node.kind = FoodKind::FruitTree;
        node.position = {x, TerrainHeight(x, z, terrainBaseY), z};
        node.maxAmount = static_cast<float>(GetRandomValue(140, 220)) * 0.01f;
        node.amount = node.maxAmount;
        node.regrowRate = static_cast<float>(GetRandomValue(1, 3)) * 0.01f;
        nodes.push_back(node);
    }

    // Water nodes at lake/sea cells - infinite refill.
    for (int x = -halfTiles; x <= halfTiles; x += 6) {
        for (int z = -halfTiles; z <= halfTiles; z += 6) {
            const float wx = static_cast<float>(x) * tileSize;
            const float wz = static_cast<float>(z) * tileSize;
            if (!IsWaterAt(wx, wz, terrainBaseY)) continue;

            FoodNode node;
            node.kind = FoodKind::Water;
            node.position = {wx, -0.36f, wz};
            node.maxAmount = 999.0f;
            node.amount = 999.0f;
            node.regrowRate = 0.0f;
            nodes.push_back(node);
        }
    }

    return nodes;
}

void UpdateFoodNodes(std::vector<FoodNode>& nodes, float dt) {
    for (FoodNode& node : nodes) {
        if (node.kind == FoodKind::Water) continue;
        if (node.amount < node.maxAmount) {
            node.amount = std::min(node.maxAmount, node.amount + node.regrowRate * dt);
        }
    }
}

void DrawFoodNode(const FoodNode& node) {
    const float fill = (node.maxAmount > 0.0f) ? Clamp(node.amount / node.maxAmount, 0.0f, 1.0f) : 0.0f;

    if (node.kind == FoodKind::BerryBush) {
        const float bushScale = 0.45f;
        DrawSphere({node.position.x, node.position.y + 0.20f, node.position.z}, bushScale,
                   Color{60, 110, 70, 255});
        // Berry clusters fade out as bush is depleted.
        const Color berry = Fade(Color{210, 60, 90, 255}, 0.55f + 0.45f * fill);
        DrawSphere({node.position.x + 0.18f, node.position.y + 0.32f, node.position.z + 0.05f}, 0.07f * (0.5f + fill * 0.7f), berry);
        DrawSphere({node.position.x - 0.16f, node.position.y + 0.36f, node.position.z + 0.10f}, 0.06f * (0.5f + fill * 0.7f), berry);
        DrawSphere({node.position.x + 0.05f, node.position.y + 0.40f, node.position.z - 0.18f}, 0.07f * (0.5f + fill * 0.7f), berry);
        DrawSphere({node.position.x - 0.09f, node.position.y + 0.30f, node.position.z - 0.14f}, 0.05f * (0.5f + fill * 0.7f), berry);
    } else if (node.kind == FoodKind::FruitTree) {
        const float trunkH = 1.4f;
        DrawCylinder({node.position.x, node.position.y + trunkH * 0.5f, node.position.z}, 0.16f, 0.16f, trunkH, 8,
                     Color{96, 70, 48, 255});
        DrawSphere({node.position.x, node.position.y + trunkH + 0.45f, node.position.z}, 0.85f, Color{72, 132, 78, 255});
        // Fruits.
        const Color fruit = Fade(Color{235, 165, 55, 255}, 0.4f + 0.6f * fill);
        const float r = 0.10f * (0.5f + fill * 0.7f);
        DrawSphere({node.position.x + 0.40f, node.position.y + trunkH + 0.30f, node.position.z + 0.20f}, r, fruit);
        DrawSphere({node.position.x - 0.35f, node.position.y + trunkH + 0.45f, node.position.z + 0.10f}, r, fruit);
        DrawSphere({node.position.x + 0.10f, node.position.y + trunkH + 0.65f, node.position.z - 0.30f}, r, fruit);
        DrawSphere({node.position.x - 0.20f, node.position.y + trunkH + 0.20f, node.position.z - 0.30f}, r, fruit);
    } else {
        // Water marker - small ripple disc to indicate drinkable spot.
        DrawCylinder({node.position.x, node.position.y + 0.03f, node.position.z}, 0.55f, 0.55f, 0.02f, 16,
                     Fade(Color{120, 200, 240, 255}, 0.55f));
    }
}
