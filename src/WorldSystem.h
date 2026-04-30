#pragma once

#include <string>
#include <vector>

#include "raylib.h"

struct WorldProp {
    Vector3 position;
    float scale;
    bool tree;
};

struct Nationality {
    std::string name;
    Color color;
    Vector3 seed;
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

void SetTerrainSeed(unsigned int seed);
float TerrainHeight(float x, float z, float terrainBaseY);
Color LerpColor(Color a, Color b, float t);
void DrawWorldTile(float x, float z, float size, float terrainBaseY, Color nationalityTint = Color{0, 0, 0, 0});

std::vector<Nationality> BuildNationalities(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed = 0);
int NationalityIdAt(float x, float z, const std::vector<Nationality>& nationalities, float terrainBaseY);
std::vector<WorldProp> BuildProps(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed = 0);
void DrawProp(const WorldProp& prop);

std::vector<FoodNode> BuildFoodNodes(int halfTiles, float tileSize, float terrainBaseY, unsigned int seed = 0);
void UpdateFoodNodes(std::vector<FoodNode>& nodes, float dt);
void DrawFoodNode(const FoodNode& node);
bool IsWaterAt(float x, float z, float terrainBaseY);
