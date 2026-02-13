#pragma once

#include "../World.h"
#include <cstdint>

class LightEngine {
public:
    static void rebuild(World *world);
    static void rebuildChunk(World *world, int cx, int cy, int cz);
    static void updateFrom(World *world, int worldX, int worldY, int worldZ);

    static void setBlockLight(World *world, int worldX, int worldY, int worldZ, uint8_t r, uint8_t g, uint8_t b);
    static void getBlockLight(World *world, int worldX, int worldY, int worldZ, uint8_t *r, uint8_t *g, uint8_t *b);

    static void setSkyLight(World *world, int worldX, int worldY, int worldZ, uint8_t level);
    static uint8_t getSkyLight(World *world, int worldX, int worldY, int worldZ);

    static void getLightLevel(World *world, int worldX, int worldY, int worldZ, uint8_t *r, uint8_t *g, uint8_t *b);

private:
    struct LightNode {
        int x;
        int y;
        int z;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    struct SkyLightNode {
        int x;
        int y;
        int z;
        uint8_t level;
    };

    static void propagateSkyLight(World *world, int cx, int cy, int cz);
    static void propagateBlockLight(World *world, int cx, int cy, int cz);
};
