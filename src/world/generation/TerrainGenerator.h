#pragma once

#include <cstdint>

#include <FastNoiseLite.h>

#include "../../utils/Random.h"
#include "../World.h"
#include "../chunk/Chunk.h"

class TerrainGenerator {
public:
    explicit TerrainGenerator(uint32_t seed);

    void generate(World &world, const ChunkPos &center);
    void generateChunk(Chunk &chunk, const ChunkPos &pos);

    int getHeightAt(int worldX, int worldZ);
    float getCaveValue(int worldX, int worldY, int worldZ);

private:
    Random m_random;
    FastNoiseLite m_warp;
    FastNoiseLite m_continental;
    FastNoiseLite m_hills;
    FastNoiseLite m_mountains;
    FastNoiseLite m_ridge;
    FastNoiseLite m_cave;
    FastNoiseLite m_tunnel;
    FastNoiseLite m_rock;
    FastNoiseLite m_surface;
};
