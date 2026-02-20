#pragma once

#include <FastNoiseLite.h>
#include <vector>

#include "../../utils/Random.h"
#include "../World.h"
#include "../biome/Biome.h"
#include "../chunk/Chunk.h"

class TerrainGenerator {
public:
    explicit TerrainGenerator(uint32_t seed);

    void generate(World &world, const ChunkPos &center);
    void generateChunk(Chunk &chunk, const ChunkPos &pos);

    int getHeightAt(int worldX, int worldZ);

    static constexpr int GRID_X  = 5;
    static constexpr int GRID_Z  = 5;
    static constexpr int GRID_Y  = 65;
    static constexpr int CELL_XZ = 4;
    static constexpr int CELL_Y  = 4;

private:
    Biome *getBiomeAt(int worldX, int worldZ) const;
    void buildDensityGrid(float *grid, int chunkX, int chunkZ);

    static constexpr float BASE_SIZE    = 17.0;
    static constexpr float STRETCH_Y    = 12.0;
    static constexpr float COORD_SCALE  = 684.412;
    static constexpr float HEIGHT_SCALE = 684.412;
    static constexpr float DEPTH_SCALE  = 200.0;

    Random m_random;

    FastNoiseLite m_temperature;
    FastNoiseLite m_humidity;

    FastNoiseLite m_lowerNoise;
    FastNoiseLite m_upperNoise;
    FastNoiseLite m_mainNoise;
    FastNoiseLite m_depthNoise;

    FastNoiseLite m_cave;
    FastNoiseLite m_tunnel;
    FastNoiseLite m_rock;
};
