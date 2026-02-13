#include "TerrainGenerator.h"

#include "../block/Block.h"
#include "../block/BlockRegistry.h"
#include "../chunk/Chunk.h"

TerrainGenerator::TerrainGenerator(uint32_t seed) : m_random(seed) {
    m_continental.SetSeed(1337);
    m_continental.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continental.SetFrequency(0.002f);

    m_mountains.SetSeed(7331);
    m_mountains.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_mountains.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_mountains.SetFractalOctaves(4);
    m_mountains.SetFrequency(0.008f);

    m_cave.SetSeed(9999);
    m_cave.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_cave.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_cave.SetFractalOctaves(3);
    m_cave.SetFrequency(0.08f);
}

int TerrainGenerator::getHeightAt(int worldX, int worldZ) {
    float cont  = m_continental.GetNoise((float) worldX, (float) worldZ);
    float mount = m_mountains.GetNoise((float) worldX, (float) worldZ);

    cont  = (cont + 1.0f) * 0.5f;
    mount = (mount + 1.0f) * 0.5f;

    float mountainMask = cont;
    mountainMask *= mountainMask;

    int height = 28 + (int) (cont * 40.0f) + (int) (mount * mountainMask * 80.0f);
    if (height < 6) height = 6;
    if (height >= Chunk::SIZE_Y) height = Chunk::SIZE_Y - 1;
    return height;
}

float TerrainGenerator::getCaveValue(int worldX, int worldY, int worldZ) { return m_cave.GetNoise((float) worldX, (float) worldY, (float) worldZ); }

void TerrainGenerator::generate(World &world, const ChunkPos &center) {
    int radius = 5;
    for (int dz = -radius; dz < radius; dz++)
        for (int dx = -radius; dx < radius; dx++) {
            ChunkPos chunkPos{center.x + dx, center.y, center.z + dz};
            Chunk &chunk = world.createChunk(chunkPos);

            for (int z = 0; z < Chunk::SIZE_Z; z++)
                for (int x = 0; x < Chunk::SIZE_X; x++) {
                    int worldX = chunkPos.x * Chunk::SIZE_X + x;
                    int worldZ = chunkPos.z * Chunk::SIZE_Z + z;

                    int height = getHeightAt(worldX, worldZ);

                    chunk.setBlock(x, 0, z, Block::byName("bedrock"));

                    for (int y = 1; y <= height; y++) {
                        if (y < height - 4) {
                            float caveValue = getCaveValue(worldX, y, worldZ);
                            float threshold = 0.35f;
                            if (caveValue > threshold) continue;
                        }

                        if (y == height) chunk.setBlock(x, y, z, Block::byName("grass"));
                        else if (y >= height - 4)
                            chunk.setBlock(x, y, z, Block::byName("dirt"));
                        else
                            chunk.setBlock(x, y, z, Block::byName("stone"));
                    }
                }
        }
}
