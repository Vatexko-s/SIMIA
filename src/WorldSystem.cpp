#include "WorldSystem.h"

#include <cmath>

#include "raymath.h"

float TerrainHeight(float x, float z, float terrainBaseY) {
    const float waves = std::sin(x * 0.22f) * 0.45f + std::cos(z * 0.19f) * 0.35f;
    const float details = std::sin((x + z) * 0.32f) * 0.22f + std::cos((x - z) * 0.27f) * 0.14f;
    return terrainBaseY + waves + details;
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

void DrawWorldTile(float x, float z, float size, float terrainBaseY) {
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

    const float thickness = h - terrainBaseY + 0.35f;
    const Vector3 tilePos = {x, terrainBaseY + thickness * 0.5f, z};
    DrawCubeV(tilePos, {size, thickness, size}, top);
    DrawCubeWiresV(tilePos, {size, thickness, size}, Fade(BLACK, 0.12f));

    if (h <= waterLevel + 0.03f) {
        DrawCubeV({x, waterLevel + 0.02f, z}, {size * 0.94f, 0.04f, size * 0.94f}, Fade(Color{95, 182, 220, 255}, 0.85f));
    }
}

std::vector<WorldProp> BuildProps(int halfTiles, float tileSize, float terrainBaseY) {
    std::vector<WorldProp> props;
    props.reserve(150);

    SetRandomSeed(1337);
    for (int i = 0; i < 150; ++i) {
        const float x = static_cast<float>(GetRandomValue(-halfTiles + 1, halfTiles - 1)) * tileSize +
                        static_cast<float>(GetRandomValue(-35, 35)) * 0.01f;
        const float z = static_cast<float>(GetRandomValue(-halfTiles + 1, halfTiles - 1)) * tileSize +
                        static_cast<float>(GetRandomValue(-35, 35)) * 0.01f;

        if (std::fabs(x - z * 0.25f) < 2.0f) continue;
        if (TerrainHeight(x, z, terrainBaseY) < -0.25f) continue;

        const bool tree = GetRandomValue(0, 100) > 35;
        const float scale = tree ? static_cast<float>(GetRandomValue(85, 135)) * 0.01f
                                 : static_cast<float>(GetRandomValue(55, 110)) * 0.01f;
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

