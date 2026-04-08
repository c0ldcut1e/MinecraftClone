#pragma once

#include <cstdint>

#include "../Level.h"
#include "../chunk/ChunkPos.h"

class LightEngine
{
public:
    struct LightNode
    {
        int x;
        int y;
        int z;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    struct SkyLightNode
    {
        int x;
        int y;
        int z;
        uint8_t level;
    };

    static void rebuild(Level *level);
    static void rebuildChunk(Level *level, const ChunkPos &pos);
    static void updateFrom(Level *level, const BlockPos &levelPos);

    static void setBlockLight(Level *level, const BlockPos &levelPos, uint8_t r, uint8_t g,
                              uint8_t b);
    static void getBlockLight(Level *level, const BlockPos &levelPos, uint8_t *r, uint8_t *g,
                              uint8_t *b);

    static void setSkyLight(Level *level, const BlockPos &levelPos, uint8_t lightLevel);
    static uint8_t getSkyLight(Level *level, const BlockPos &levelPos);

    static void getLightLevel(Level *level, const BlockPos &levelPos, uint8_t *r, uint8_t *g,
                              uint8_t *b);

private:
    static void propagateSkyLight(Level *level, const ChunkPos &pos);
    static void propagateBlockLight(Level *level, const ChunkPos &pos);
};
