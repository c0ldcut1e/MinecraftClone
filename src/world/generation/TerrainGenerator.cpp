#include "TerrainGenerator.h"

#include "../block/Block.h"
#include "../block/BlockRegistry.h"
#include "../chunk/Chunk.h"

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float smooth01(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

TerrainGenerator::TerrainGenerator(uint32_t seed) : m_random(seed) {
    int s0 = (int) seed;
    int s1 = (int) (seed * 747796405u + 2891336453u);
    int s2 = (int) (seed * 277803737u + 187599719u);
    int s3 = (int) (seed * 362437u + 1013904223u);
    int s4 = (int) (seed * 2654435761u + 2246822519u);
    int s5 = (int) (seed * 1597334677u + 3266489917u);
    int s6 = (int) (seed * 3812015801u + 668265263u);
    int s7 = (int) (seed * 1103515245u + 12345u);
    int s8 = (int) (seed * 97531u + 864197532u);

    m_warp.SetSeed(s0 ^ 0x4b1d);
    m_warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_warp.SetFrequency(0.0012f);

    m_continental.SetSeed(s1 ^ 0x1337);
    m_continental.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_continental.SetFrequency(0.0012f);

    m_hills.SetSeed(s2 ^ 0x22aa);
    m_hills.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_hills.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_hills.SetFractalOctaves(3);
    m_hills.SetFrequency(0.0052f);

    m_mountains.SetSeed(s3 ^ 0x7331);
    m_mountains.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_mountains.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_mountains.SetFractalOctaves(4);
    m_mountains.SetFrequency(0.0076f);

    m_ridge.SetSeed(s4 ^ 0x5a5a);
    m_ridge.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_ridge.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_ridge.SetFractalOctaves(2);
    m_ridge.SetFrequency(0.010f);

    m_cave.SetSeed(s5 ^ 0x9999);
    m_cave.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_cave.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_cave.SetFractalOctaves(3);
    m_cave.SetFrequency(0.06f);

    m_tunnel.SetSeed(s6 ^ 0x5555);
    m_tunnel.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_tunnel.SetFrequency(0.02f);

    m_rock.SetSeed(s7 ^ 0x4242);
    m_rock.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_rock.SetFrequency(9.0f);

    m_surface.SetSeed(s8 ^ 0x2024);
    m_surface.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_surface.SetFrequency(0.012f);
}

int TerrainGenerator::getHeightAt(int worldX, int worldZ) {
    float wx = (float) worldX;
    float wz = (float) worldZ;

    float warpX = m_warp.GetNoise(wx, wz);
    float warpZ = m_warp.GetNoise(wx + 137.0f, wz - 91.0f);

    wx += warpX * 72.0f;
    wz += warpZ * 72.0f;

    float continental = m_continental.GetNoise(wx, wz);
    float hills       = m_hills.GetNoise(wx, wz);
    float mountains   = m_mountains.GetNoise(wx, wz);
    float ridgeBase   = m_ridge.GetNoise(wx, wz);
    float surface     = m_surface.GetNoise(wx, wz);

    continental = (continental + 1.0f) * 0.5f;
    hills       = (hills + 1.0f) * 0.5f;
    mountains   = (mountains + 1.0f) * 0.5f;
    surface     = (surface + 1.0f) * 0.5f;

    float ridge = 1.0f - (ridgeBase < 0.0f ? -ridgeBase : ridgeBase);
    ridge       = clamp01(ridge);
    ridge       = ridge * ridge;

    float land     = smooth01((continental - 0.24f) / 0.76f);
    float plateau  = smooth01((continental - 0.48f) / 0.34f);
    float peakMask = land * (0.25f + 0.75f * plateau);

    int baseHeight = 11 + (int) (land * 56.0f);

    int hillHeight = (int) ((hills - 0.5f) * (10.0f + 14.0f * land));
    int ridgeLift  = (int) (ridge * peakMask * 22.0f);
    int peakLift   = (int) (mountains * peakMask * 72.0f);

    int height = baseHeight + hillHeight + ridgeLift + peakLift;

    height += (int) ((surface - 0.5f) * (5.0f + 4.0f * land));

    int hardMax = Chunk::SIZE_Y - 5;

    if (height > hardMax) {
        int over = height - hardMax;

        float pinch = 1.0f - smooth01((float) over / 22.0f);
        if (pinch < 0.0f) pinch = 0.0f;

        float capNoise = m_surface.GetNoise(wx * 0.7f, wz * 0.7f);
        capNoise       = (capNoise + 1.0f) * 0.5f;

        int capJitter = (int) ((capNoise - 0.5f) * 7.0f);

        height = hardMax - (int) ((1.0f - pinch) * 18.0f) + capJitter;
        if (height > hardMax) height = hardMax;
    }

    if (height < 6) height = 6;
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
            int surfaceProtect = 9;

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

                if (y == height) chunk.setBlock(x, y, z, Block::byName("grass"));
                else if (y >= height - 4)
                    chunk.setBlock(x, y, z, Block::byName("dirt"));
                else {
                    float rockNoise = m_rock.GetNoise((float) worldX, (float) y, (float) worldZ);
                    rockNoise       = (rockNoise + 1.0f) * 0.5f;
                    if (rockNoise > 0.62f) chunk.setBlock(x, y, z, Block::byName("andesite"));
                    else
                        chunk.setBlock(x, y, z, Block::byName("stone"));
                }
            }
        }
}

void TerrainGenerator::generate(World &world, const ChunkPos &center) {
    int radius = 1;

    for (int dz = -radius; dz < radius; dz++)
        for (int dx = -radius; dx < radius; dx++) {
            ChunkPos chunkPos{center.x + dx, center.y, center.z + dz};
            Chunk &chunk = world.createChunk(chunkPos);
            generateChunk(chunk, chunkPos);
        }
}
