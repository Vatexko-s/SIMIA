#pragma once

#include <vector>

#include "raylib.h"

struct WorldProp {
    Vector3 position;
    float scale;
    bool tree;
};

float TerrainHeight(float x, float z, float terrainBaseY);
Color LerpColor(Color a, Color b, float t);
void DrawWorldTile(float x, float z, float size, float terrainBaseY);
std::vector<WorldProp> BuildProps(int halfTiles, float tileSize, float terrainBaseY);
void DrawProp(const WorldProp& prop);

