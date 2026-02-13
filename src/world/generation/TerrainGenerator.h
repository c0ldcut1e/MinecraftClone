#pragma once

#include <FastNoiseLite.h>

#include "../../utils/Random.h"
#include "../World.h"

class TerrainGenerator {
public:
    explicit TerrainGenerator(uint32_t seed);

    void generate(World &world, const ChunkPos &center);
    int getHeightAt(int worldX, int worldZ);
    float getCaveValue(int worldX, int worldY, int worldZ);

private:
    Random m_random;
    FastNoiseLite m_continental;
    FastNoiseLite m_mountains;
    FastNoiseLite m_cave;
};
