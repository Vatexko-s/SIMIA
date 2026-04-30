#pragma once

#include <vector>

#include "raylib.h"

struct WorldProp {
    Vector3 position;
    float scale;
    bool tree;
};

enum class FoodKind {
    BerryBush,
    FruitTree,
    Water
};

struct FoodNode {
    Vector3 position;
    FoodKind kind = FoodKind::BerryBush;
    float amount = 1.0f;       // Current available 0..maxAmount.
    float maxAmount = 1.0f;    // Capacity.
    float regrowRate = 0.05f;  // Units per second.
};

float TerrainHeight(float x, float z, float terrainBaseY);
Color LerpColor(Color a, Color b, float t);
void DrawWorldTile(float x, float z, float size, float terrainBaseY);
std::vector<WorldProp> BuildProps(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed = 0);
void DrawProp(const WorldProp& prop);

std::vector<FoodNode> BuildFoodNodes(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed = 0);
void UpdateFoodNodes(std::vector<FoodNode>& nodes, float dt);
void DrawFoodNode(const FoodNode& node);
bool IsWaterAt(float x, float z, float terrainBaseY);
