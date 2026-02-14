#pragma once

#include <cstdint>

#include "../World.h"
#include "../chunk/ChunkPos.h"

class LightEngine {
public:
    static void rebuild(World *world);
    static void rebuildChunk(World *world, const ChunkPos &pos);
    static void updateFrom(World *world, const BlockPos &worldPos);

    static void setBlockLight(World *world, const BlockPos &worldPos, uint8_t r, uint8_t g, uint8_t b);
    static void getBlockLight(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b);

    static void setSkyLight(World *world, const BlockPos &worldPos, uint8_t level);
    static uint8_t getSkyLight(World *world, const BlockPos &worldPos);

    static void getLightLevel(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b);

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

    static void propagateSkyLight(World *world, const ChunkPos &pos);
    static void propagateBlockLight(World *world, const ChunkPos &pos);
};
