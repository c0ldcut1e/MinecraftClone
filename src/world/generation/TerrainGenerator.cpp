#include "TerrainGenerator.h"

#include "../../utils/math/Math.h"
#include "../biome/BiomeRegistry.h"
#include "../block/Block.h"
#include "../chunk/Chunk.h"

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static float lerpd(float a, float b, float t) { return a + (b - a) * t; }

static inline int gridIdx(int gx, int gy, int gz) { return (gx * TerrainGenerator::GRID_Z + gz) * TerrainGenerator::GRID_Y + gy; }

TerrainGenerator::TerrainGenerator(uint32_t seed) : m_random(seed) {
    auto ds = [](uint32_t s, uint32_t m, uint32_t a) { return (int) (s * m + a); };

    int s0 = ds(seed, 1u, 0u);
    int s1 = ds(seed, 747796405u, 2891336453u);
    int s2 = ds(seed, 277803737u, 187599719u);
    int s3 = ds(seed, 362437u, 1013904223u);
    int s4 = ds(seed, 2654435761u, 2246822519u);
    int s5 = ds(seed, 1597334677u, 3266489917u);
    int s6 = ds(seed, 2246822519u, 747796405u);
    int s7 = ds(seed, 3266489917u, 1597334677u);
    int s8 = ds(seed, 1013904223u, 362437u);

    m_temperature.SetSeed(s0 ^ 0x1a2b);
    m_temperature.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_temperature.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_temperature.SetFractalOctaves(4);
    m_temperature.SetFrequency(1.0f / (float) DEPTH_SCALE);

    m_humidity.SetSeed(s1 ^ 0x3c4d);
    m_humidity.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_humidity.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_humidity.SetFractalOctaves(4);
    m_humidity.SetFrequency(1.0f / (float) DEPTH_SCALE);

    m_lowerNoise.SetSeed(s2 ^ 0x5e6f);
    m_lowerNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_lowerNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_lowerNoise.SetFractalOctaves(16);
    m_lowerNoise.SetFrequency(1.0f);

    m_upperNoise.SetSeed(s3 ^ 0x7a8b);
    m_upperNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_upperNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_upperNoise.SetFractalOctaves(16);
    m_upperNoise.SetFrequency(1.0f);

    m_mainNoise.SetSeed(s4 ^ 0x9c0d);
    m_mainNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_mainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_mainNoise.SetFractalOctaves(8);
    m_mainNoise.SetFrequency(1.0f);

    m_depthNoise.SetSeed(s5 ^ 0xb2e1);
    m_depthNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_depthNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_depthNoise.SetFractalOctaves(16);
    m_depthNoise.SetFrequency(1.0f);

    m_cave.SetSeed(s6 ^ 0x9999);
    m_cave.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_cave.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_cave.SetFractalOctaves(3);
    m_cave.SetFrequency(0.06f);

    m_tunnel.SetSeed(s7 ^ 0x5555);
    m_tunnel.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_tunnel.SetFrequency(0.02f);

    m_rock.SetSeed(s8 ^ 0x4242);
    m_rock.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_rock.SetFrequency(9.0f);
}

Biome *TerrainGenerator::getBiomeAt(int worldX, int worldZ) const {
    float temp  = m_temperature.GetNoise((float) worldX, (float) worldZ);
    float humid = m_humidity.GetNoise((float) worldX, (float) worldZ);
    temp        = (temp + 1.0f) * 0.5f;
    humid       = (humid + 1.0f) * 0.5f;
    if (temp > 0.55f) return Biome::byName("desert");
    return Biome::byName("plains");
}

void TerrainGenerator::buildDensityGrid(float *grid, int chunkX, int chunkZ) {
    for (int gx = 0; gx < GRID_X; gx++) {
        int worldX = chunkX * Chunk::SIZE_X + gx * CELL_XZ;

        for (int gz = 0; gz < GRID_Z; gz++) {
            int worldZ = chunkZ * Chunk::SIZE_Z + gz * CELL_XZ;

            float depthRaw = m_depthNoise.GetNoise((float) (worldX / DEPTH_SCALE), (float) (worldZ / DEPTH_SCALE));
            depthRaw       = Math::clamp(depthRaw, -1.0, 1.0);

            for (int gy = 0; gy < GRID_Y; gy++) {
                float worldY = gy * CELL_Y;

                float nx = worldX / COORD_SCALE;
                float ny = worldY / HEIGHT_SCALE;
                float nz = worldZ / COORD_SCALE;

                float nxMain = worldX / (COORD_SCALE / 80.0);
                float nyMain = worldY / (HEIGHT_SCALE / 160.0);
                float nzMain = worldZ / (COORD_SCALE / 80.0);

                float lower = m_lowerNoise.GetNoise((float) nx, (float) ny, (float) nz) * 512.0;
                float upper = m_upperNoise.GetNoise((float) nx, (float) ny, (float) nz) * 512.0;
                float main  = (m_mainNoise.GetNoise((float) nxMain, (float) nyMain, (float) nzMain) + 1.0) * 0.5;
                main        = Math::clamp(main, 0.0, 1.0);

                float density = lerpd(lower, upper, main);

                float yBias = (BASE_SIZE - (float) gy) / STRETCH_Y;
                yBias += depthRaw;

                density = density / 512.0 + yBias;

                grid[gridIdx(gx, gy, gz)] = density;
            }
        }
    }
}

int TerrainGenerator::getHeightAt(int worldX, int worldZ) {
    int chunkX = worldX >> 4;
    int chunkZ = worldZ >> 4;
    int lx     = worldX & 15;
    int lz     = worldZ & 15;

    float grid[GRID_X * GRID_Y * GRID_Z];
    buildDensityGrid(grid, chunkX, chunkZ);

    int gx0  = lx / CELL_XZ;
    int gz0  = lz / CELL_XZ;
    int gx1  = gx0 + 1;
    int gz1  = gz0 + 1;
    float tx = (float) (lx % CELL_XZ) / CELL_XZ;
    float tz = (float) (lz % CELL_XZ) / CELL_XZ;

    for (int y = Chunk::SIZE_Y - 1; y >= 1; y--) {
        int gy0  = y / CELL_Y;
        int gy1  = gy0 + 1;
        float ty = (float) (y % CELL_Y) / CELL_Y;
        if (gy1 >= GRID_Y) continue;

        float d000 = grid[gridIdx(gx0, gy0, gz0)];
        float d100 = grid[gridIdx(gx1, gy0, gz0)];
        float d010 = grid[gridIdx(gx0, gy1, gz0)];
        float d110 = grid[gridIdx(gx1, gy1, gz0)];
        float d001 = grid[gridIdx(gx0, gy0, gz1)];
        float d101 = grid[gridIdx(gx1, gy0, gz1)];
        float d011 = grid[gridIdx(gx0, gy1, gz1)];
        float d111 = grid[gridIdx(gx1, gy1, gz1)];

        float c00     = lerpd(d000, d100, tx);
        float c10     = lerpd(d010, d110, tx);
        float c01     = lerpd(d001, d101, tx);
        float c11     = lerpd(d011, d111, tx);
        float c0      = lerpd(c00, c01, tz);
        float c1      = lerpd(c10, c11, tz);
        float density = lerpd(c0, c1, ty);

        if (density > 0.0) return y;
    }
    return 0;
}

static float caveValueAt(FastNoiseLite &cave, FastNoiseLite &tunnel, int x, int y, int z) {
    float cheese = cave.GetNoise((float) x, (float) y, (float) z);
    float tube   = tunnel.GetNoise((float) x, (float) y * 0.5f, (float) z);
    return cheese - tube * 0.6f;
}

void TerrainGenerator::generateChunk(Chunk &chunk, const ChunkPos &chunkPos) {
    Block *bedrock  = Block::byName("bedrock");
    Block *stone    = Block::byName("stone");
    Block *andesite = Block::byName("andesite");
    Block *sand     = Block::byName("sand");
    Block *gravel   = Block::byName("gravel");
    Block *water    = Block::byName("air");

    const int seaLevel    = 64;
    const int bedrockCeil = 5;

    float grid[GRID_X * GRID_Y * GRID_Z];
    buildDensityGrid(grid, chunkPos.x, chunkPos.z);

    for (int z = 0; z < Chunk::SIZE_Z; z++) {
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            int worldX = chunkPos.x * Chunk::SIZE_X + x;
            int worldZ = chunkPos.z * Chunk::SIZE_Z + z;

            int gx0  = x / CELL_XZ;
            int gz0  = z / CELL_XZ;
            int gx1  = gx0 + 1;
            int gz1  = gz0 + 1;
            float tx = (float) (x % CELL_XZ) / CELL_XZ;
            float tz = (float) (z % CELL_XZ) / CELL_XZ;

            int height = 0;
            for (int y = Chunk::SIZE_Y - 1; y >= 1; y--) {
                int gy0  = y / CELL_Y;
                int gy1  = gy0 + 1;
                float ty = (float) (y % CELL_Y) / CELL_Y;
                if (gy1 >= GRID_Y) continue;

                float d000 = grid[gridIdx(gx0, gy0, gz0)];
                float d100 = grid[gridIdx(gx1, gy0, gz0)];
                float d010 = grid[gridIdx(gx0, gy1, gz0)];
                float d110 = grid[gridIdx(gx1, gy1, gz0)];
                float d001 = grid[gridIdx(gx0, gy0, gz1)];
                float d101 = grid[gridIdx(gx1, gy0, gz1)];
                float d011 = grid[gridIdx(gx0, gy1, gz1)];
                float d111 = grid[gridIdx(gx1, gy1, gz1)];

                float c00     = lerpd(d000, d100, tx);
                float c10     = lerpd(d010, d110, tx);
                float c01     = lerpd(d001, d101, tx);
                float c11     = lerpd(d011, d111, tx);
                float c0      = lerpd(c00, c01, tz);
                float c1      = lerpd(c10, c11, tz);
                float density = lerpd(c0, c1, ty);

                if (density > 0.0 && height == 0) {
                    height = y;
                    break;
                }
            }

            Biome *biome = getBiomeAt(worldX, worldZ);
            if (!biome) biome = Biome::byName("plains");

            Block *top;
            Block *filler;
            if (height < seaLevel - 1) {
                top    = gravel;
                filler = gravel;
            } else if (height <= seaLevel + 1) {
                top    = sand;
                filler = sand;
            } else {
                top    = biome->getTopBlock();
                filler = biome->getFillerBlock();
            }

            const int surfaceProtect = 8;

            for (int y = 1; y < Chunk::SIZE_Y; y++) {
                int gy0  = y / CELL_Y;
                int gy1  = gy0 + 1;
                float ty = (float) (y % CELL_Y) / CELL_Y;
                if (gy1 >= GRID_Y) continue;

                float d000 = grid[gridIdx(gx0, gy0, gz0)];
                float d100 = grid[gridIdx(gx1, gy0, gz0)];
                float d010 = grid[gridIdx(gx0, gy1, gz0)];
                float d110 = grid[gridIdx(gx1, gy1, gz0)];
                float d001 = grid[gridIdx(gx0, gy0, gz1)];
                float d101 = grid[gridIdx(gx1, gy0, gz1)];
                float d011 = grid[gridIdx(gx0, gy1, gz1)];
                float d111 = grid[gridIdx(gx1, gy1, gz1)];

                float c00     = lerpd(d000, d100, tx);
                float c10     = lerpd(d010, d110, tx);
                float c01     = lerpd(d001, d101, tx);
                float c11     = lerpd(d011, d111, tx);
                float c0      = lerpd(c00, c01, tz);
                float c1      = lerpd(c10, c11, tz);
                float density = lerpd(c0, c1, ty);

                if (density <= 0.0) continue;

                if (y < bedrockCeil) {
                    float bt = (float) y / (float) bedrockCeil;
                    float r  = m_rock.GetNoise((float) worldX * 0.2f, (float) y * 2.0f, (float) worldZ * 0.2f);
                    r        = (r + 1.0f) * 0.5f;
                    if (r < 1.0f - bt * bt) {
                        chunk.setBlock(x, y, z, bedrock);
                        continue;
                    }
                }

                bool inShell = y >= height - surfaceProtect;
                if (!inShell && y > bedrockCeil + 2) {
                    float cv   = caveValueAt(m_cave, m_tunnel, worldX, y, worldZ);
                    float fade = Math::clampf((float) (y - (bedrockCeil + 2)) / 12.0f, 0.0f, 1.0f);
                    if (cv > lerpf(0.55f, 0.35f, fade)) continue;
                }

                if (y == height) chunk.setBlock(x, y, z, top);
                else if (y >= height - 4)
                    chunk.setBlock(x, y, z, filler);
                else {
                    float r = m_rock.GetNoise((float) worldX, (float) y, (float) worldZ);
                    r       = (r + 1.0f) * 0.5f;
                    if (r > 0.62f) chunk.setBlock(x, y, z, andesite);
                    else
                        chunk.setBlock(x, y, z, stone);
                }
            }

            if (height < seaLevel) {
                int wy0 = (height <= 0) ? 1 : (height + 1);
                for (int wy = wy0; wy <= seaLevel && wy < Chunk::SIZE_Y; wy++) { chunk.setBlock(x, wy, z, water); }
            }

            chunk.setBlock(x, 0, z, bedrock);
        }
    }
}

void TerrainGenerator::generate(World &world, const ChunkPos &center) {
    int radius = 1;
    for (int z = -radius; z <= radius; z++)
        for (int x = -radius; x <= radius; x++) {
            ChunkPos pos(center.x + x, 0, center.z + z);
            Chunk *chunk = world.getChunk(pos);
            if (!chunk) {
                world.createChunk(pos);
                chunk = world.getChunk(pos);
            }
            if (chunk) generateChunk(*chunk, pos);
        }
}
