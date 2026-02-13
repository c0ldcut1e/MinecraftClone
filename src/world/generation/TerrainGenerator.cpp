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
    m_cave.SetFrequency(0.06f);

    m_tunnel.SetSeed(5555);
    m_tunnel.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_tunnel.SetFrequency(0.02f);

    m_rock.SetSeed(4242);
    m_rock.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_rock.SetFrequency(10.0f);

    m_surface.SetSeed(2024);
    m_surface.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_surface.SetFrequency(0.01f);
}

int TerrainGenerator::getHeightAt(int worldX, int worldZ) {
    float cont  = m_continental.GetNoise((float) worldX, (float) worldZ);
    float mount = m_mountains.GetNoise((float) worldX, (float) worldZ);
    float surf  = m_surface.GetNoise((float) worldX, (float) worldZ);

    cont  = (cont + 1.0f) * 0.5f;
    mount = (mount + 1.0f) * 0.5f;
    surf  = (surf + 1.0f) * 0.5f;

    float mountainMask = cont * cont;

    int height = 28 + (int) (cont * 40.0f) + (int) (mount * mountainMask * 80.0f);
    height += (int) ((surf - 0.5f) * 8.0f);

    if (height < 6) height = 6;
    if (height >= Chunk::SIZE_Y) height = Chunk::SIZE_Y - 1;

    return height;
}

float TerrainGenerator::getCaveValue(int worldX, int worldY, int worldZ) {
    float cheese = m_cave.GetNoise((float) worldX, (float) worldY, (float) worldZ);
    float tunnel = m_tunnel.GetNoise((float) worldX, (float) worldY * 0.5f, (float) worldZ);
    return cheese - (tunnel * 0.6f);
}

void TerrainGenerator::generateChunk(Chunk &chunk, const ChunkPos &chunkPos) {
    for (int z = 0; z < Chunk::SIZE_Z; z++)
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            int worldX = chunkPos.x * Chunk::SIZE_X + x;
            int worldZ = chunkPos.z * Chunk::SIZE_Z + z;

            int height = getHeightAt(worldX, worldZ);

            int bedrockCeil    = 5;
            int surfaceProtect = 8;

            for (int y = 0; y < Chunk::SIZE_Y; y++) {
                if (y == 0) {
                    chunk.setBlock(x, y, z, Block::byName("bedrock"));
                    continue;
                }

                if (y < bedrockCeil) {
                    float t = (float) y / (float) bedrockCeil;
                    float r = m_rock.GetNoise((float) worldX * 0.2f, (float) y * 2.0f, (float) worldZ * 0.2f);
                    r       = (r + 1.0f) * 0.5f;

                    float fill = 1.0f - (t * t);
                    if (r < fill) {
                        chunk.setBlock(x, y, z, Block::byName("bedrock"));
                        continue;
                    }
                }

                if (y > height) continue;

                bool inSurfaceShell = (y >= height - surfaceProtect);

                if (!inSurfaceShell && y > bedrockCeil + 2) {
                    float caveValue = getCaveValue(worldX, y, worldZ);

                    float depth    = (float) (y - (bedrockCeil + 2));
                    float fadeSpan = 12.0f;
                    float fade     = depth / fadeSpan;
                    if (fade < 0.0f) fade = 0.0f;
                    if (fade > 1.0f) fade = 1.0f;

                    float thresholdTop  = 0.55f;
                    float thresholdDeep = 0.35f;
                    float threshold     = thresholdTop + (thresholdDeep - thresholdTop) * fade;

                    if (caveValue > threshold) continue;
                }

                if (y == height) {
                    chunk.setBlock(x, y, z, Block::byName("grass"));
                } else if (y >= height - 4) {
                    chunk.setBlock(x, y, z, Block::byName("dirt"));
                } else {
                    float rockNoise = m_rock.GetNoise((float) worldX, (float) y, (float) worldZ);
                    rockNoise       = (rockNoise + 1.0f) * 0.5f;

                    if (rockNoise > 0.6f) chunk.setBlock(x, y, z, Block::byName("andesite"));
                    else
                        chunk.setBlock(x, y, z, Block::byName("stone"));
                }
            }
        }
}

void TerrainGenerator::generate(World &world, const ChunkPos &center) {
    int radius = 5;

    for (int dz = -radius; dz < radius; dz++)
        for (int dx = -radius; dx < radius; dx++) {
            ChunkPos chunkPos{center.x + dx, center.y, center.z + dz};
            Chunk &chunk = world.createChunk(chunkPos);
            generateChunk(chunk, chunkPos);
        }
}
