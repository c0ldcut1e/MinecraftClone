#include "TerrainGenerator.h"

#include <cmath>

#include "../../utils/math/Mth.h"
#include "../biome/BiomeRegistry.h"
#include "../block/Block.h"
#include "../chunk/Chunk.h"

static inline int getGridIndex(int gx, int gy, int gz)
{
    return (gx * TerrainGenerator::GRID_Z + gz) * TerrainGenerator::GRID_Y + gy;
}

static inline uint64_t mix64(uint64_t value)
{
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

static inline uint32_t seedCave(uint32_t seed, int chunkX, int chunkZ)
{
    uint64_t value = (uint64_t) (uint32_t) chunkX * 341873128712ULL +
                     (uint64_t) (uint32_t) chunkZ * 132897987541ULL;
    value ^= (uint64_t) seed * 0x9e3779b97f4a7c15ULL;
    return (uint32_t) mix64(value);
}

static inline int getRandomIndex(Random &random, int maxExclusive)
{
    if (maxExclusive <= 1)
    {
        return 0;
    }

    return random.nextInt(0, maxExclusive - 1);
}

TerrainGenerator::TerrainGenerator(uint32_t seed) : m_seed(seed), m_random(seed)
{
    auto deriveSeed = [](uint32_t s, uint32_t m, uint32_t a) { return (int) (s * m + a); };
    int s0          = deriveSeed(seed, 1u, 0u);
    int s1          = deriveSeed(seed, 747796405u, 2891336453u);
    int s2          = deriveSeed(seed, 277803737u, 187599719u);
    int s3          = deriveSeed(seed, 362437u, 1013904223u);
    int s4          = deriveSeed(seed, 2654435761u, 2246822519u);
    int s5          = deriveSeed(seed, 1597334677u, 3266489917u);
    int s6          = deriveSeed(seed, 2246822519u, 747796405u);
    int s7          = deriveSeed(seed, 3266489917u, 1597334677u);
    int s8          = deriveSeed(seed, 1013904223u, 362437u);

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
    m_cave.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_cave.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_cave.SetFractalOctaves(3);
    m_cave.SetFrequency(0.06f);

    m_tunnel.SetSeed(s7 ^ 0x5555);
    m_tunnel.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_tunnel.SetFrequency(0.02f);

    m_rock.SetSeed(s8 ^ 0x4242);
    m_rock.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_rock.SetFrequency(9.0f);
}

void TerrainGenerator::generate(Level &level, const ChunkPos &center)
{
    ChunkPos pos(center.x, 0, center.z);
    Chunk *chunk = level.getChunk(pos);
    if (!chunk)
    {
        level.createChunk(pos);
        chunk = level.getChunk(pos);
    }

    if (chunk)
    {
        generateChunk(*chunk, pos);
    }
}

void TerrainGenerator::generateChunk(Chunk &chunk, const ChunkPos &chunkPos)
{
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

    int heightMap[Chunk::SIZE_X * Chunk::SIZE_Z];

    for (int z = 0; z < Chunk::SIZE_Z; z++)
    {
        for (int x = 0; x < Chunk::SIZE_X; x++)
        {
            int gx0  = x / CELL_XZ;
            int gz0  = z / CELL_XZ;
            int gx1  = gx0 + 1;
            int gz1  = gz0 + 1;
            float tx = (float) (x % CELL_XZ) / CELL_XZ;
            float tz = (float) (z % CELL_XZ) / CELL_XZ;

            int height = 0;
            for (int y = Chunk::SIZE_Y - 1; y >= 1; y--)
            {
                int gy0  = y / CELL_Y;
                int gy1  = gy0 + 1;
                float ty = (float) (y % CELL_Y) / CELL_Y;
                if (gy1 >= GRID_Y)
                    continue;

                float d000 = grid[getGridIndex(gx0, gy0, gz0)];
                float d100 = grid[getGridIndex(gx1, gy0, gz0)];
                float d010 = grid[getGridIndex(gx0, gy1, gz0)];
                float d110 = grid[getGridIndex(gx1, gy1, gz0)];
                float d001 = grid[getGridIndex(gx0, gy0, gz1)];
                float d101 = grid[getGridIndex(gx1, gy0, gz1)];
                float d011 = grid[getGridIndex(gx0, gy1, gz1)];
                float d111 = grid[getGridIndex(gx1, gy1, gz1)];

                float c00     = Mth::lerpf(d000, d100, tx);
                float c10     = Mth::lerpf(d010, d110, tx);
                float c01     = Mth::lerpf(d001, d101, tx);
                float c11     = Mth::lerpf(d011, d111, tx);
                float c0      = Mth::lerpf(c00, c01, tz);
                float c1      = Mth::lerpf(c10, c11, tz);
                float density = Mth::lerpf(c0, c1, ty);

                if (density > 0.0f && height == 0)
                {
                    height = y;
                    break;
                }
            }

            heightMap[x + z * Chunk::SIZE_X] = height;
        }
    }

    for (int z = 0; z < Chunk::SIZE_Z; z++)
    {
        for (int x = 0; x < Chunk::SIZE_X; x++)
        {
            int levelX = chunkPos.x * Chunk::SIZE_X + x;
            int levelZ = chunkPos.z * Chunk::SIZE_Z + z;

            int gx0  = x / CELL_XZ;
            int gz0  = z / CELL_XZ;
            int gx1  = gx0 + 1;
            int gz1  = gz0 + 1;
            float tx = (float) (x % CELL_XZ) / CELL_XZ;
            float tz = (float) (z % CELL_XZ) / CELL_XZ;

            int height = heightMap[x + z * Chunk::SIZE_X];

            Biome *biome = getBiomeAt(levelX, levelZ);
            if (!biome)
                biome = Biome::byName("plains");

            chunk.setBiomeAt(x, z, biome);

            Block *top;
            Block *filler;
            if (height < seaLevel - 1)
            {
                top    = gravel;
                filler = gravel;
            }
            else if (height <= seaLevel + 1)
            {
                top    = sand;
                filler = sand;
            }
            else
            {
                top    = biome->getTopBlock();
                filler = biome->getFillerBlock();
            }

            for (int y = 1; y < Chunk::SIZE_Y; y++)
            {
                int gy0  = y / CELL_Y;
                int gy1  = gy0 + 1;
                float ty = (float) (y % CELL_Y) / CELL_Y;
                if (gy1 >= GRID_Y)
                {
                    continue;
                }

                float d000 = grid[getGridIndex(gx0, gy0, gz0)];
                float d100 = grid[getGridIndex(gx1, gy0, gz0)];
                float d010 = grid[getGridIndex(gx0, gy1, gz0)];
                float d110 = grid[getGridIndex(gx1, gy1, gz0)];
                float d001 = grid[getGridIndex(gx0, gy0, gz1)];
                float d101 = grid[getGridIndex(gx1, gy0, gz1)];
                float d011 = grid[getGridIndex(gx0, gy1, gz1)];
                float d111 = grid[getGridIndex(gx1, gy1, gz1)];

                float c00     = Mth::lerpf(d000, d100, tx);
                float c10     = Mth::lerpf(d010, d110, tx);
                float c01     = Mth::lerpf(d001, d101, tx);
                float c11     = Mth::lerpf(d011, d111, tx);
                float c0      = Mth::lerpf(c00, c01, tz);
                float c1      = Mth::lerpf(c10, c11, tz);
                float density = Mth::lerpf(c0, c1, ty);

                if (density <= 0.0f)
                {
                    continue;
                }

                if (y < bedrockCeil)
                {
                    float bt        = (float) y / (float) bedrockCeil;
                    float rockNoise = m_rock.GetNoise((float) levelX * 0.2f, (float) y * 2.0f,
                                                      (float) levelZ * 0.2f);
                    rockNoise       = (rockNoise + 1.0f) * 0.5f;
                    if (rockNoise < 1.0f - bt * bt)
                    {
                        chunk.setBlock(x, y, z, bedrock);
                        continue;
                    }
                }

                if (y == height)
                {
                    chunk.setBlock(x, y, z, top);
                }
                else if (y >= height - 4)
                {
                    chunk.setBlock(x, y, z, filler);
                }
                else
                {
                    float rock = m_rock.GetNoise((float) levelX, (float) y, (float) levelZ);
                    rock       = (rock + 1.0f) * 0.5f;
                    if (rock > 0.62f)
                    {
                        chunk.setBlock(x, y, z, andesite);
                    }
                    else
                    {
                        chunk.setBlock(x, y, z, stone);
                    }
                }
            }

            chunk.setBlock(x, 0, z, bedrock);
        }
    }

    for (int z = 0; z < Chunk::SIZE_Z; z++)
    {
        for (int x = 0; x < Chunk::SIZE_X; x++)
        {
            int height = heightMap[x + z * Chunk::SIZE_X];
            if (height < seaLevel)
            {
                int wy0 = (height <= 0) ? 1 : (height + 1);
                for (int wy = wy0; wy <= seaLevel && wy < Chunk::SIZE_Y; wy++)
                {
                    chunk.setBlock(x, wy, z, water);
                }
            }
        }
    }

    static constexpr int caveNeighborRadius = 3;
    for (int neighborZ = -caveNeighborRadius; neighborZ <= caveNeighborRadius; neighborZ++)
    {
        for (int neighborX = -caveNeighborRadius; neighborX <= caveNeighborRadius; neighborX++)
        {
            int sourceChunkX = chunkPos.x + neighborX;
            int sourceChunkZ = chunkPos.z + neighborZ;
            carveCavesFromSourceChunk(chunk, chunkPos, sourceChunkX, sourceChunkZ);
        }
    }
}

int TerrainGenerator::getHeightAt(int levelX, int levelZ)
{
    int chunkX = levelX >> 4;
    int chunkZ = levelZ >> 4;
    int lx     = levelX & 15;
    int lz     = levelZ & 15;

    float grid[GRID_X * GRID_Y * GRID_Z];
    buildDensityGrid(grid, chunkX, chunkZ);

    int gx0  = lx / CELL_XZ;
    int gz0  = lz / CELL_XZ;
    int gx1  = gx0 + 1;
    int gz1  = gz0 + 1;
    float tx = (float) (lx % CELL_XZ) / CELL_XZ;
    float tz = (float) (lz % CELL_XZ) / CELL_XZ;

    for (int y = Chunk::SIZE_Y - 1; y >= 1; y--)
    {
        int gy0  = y / CELL_Y;
        int gy1  = gy0 + 1;
        float ty = (float) (y % CELL_Y) / CELL_Y;
        if (gy1 >= GRID_Y)
        {
            continue;
        }

        float d000 = grid[getGridIndex(gx0, gy0, gz0)];
        float d100 = grid[getGridIndex(gx1, gy0, gz0)];
        float d010 = grid[getGridIndex(gx0, gy1, gz0)];
        float d110 = grid[getGridIndex(gx1, gy1, gz0)];
        float d001 = grid[getGridIndex(gx0, gy0, gz1)];
        float d101 = grid[getGridIndex(gx1, gy0, gz1)];
        float d011 = grid[getGridIndex(gx0, gy1, gz1)];
        float d111 = grid[getGridIndex(gx1, gy1, gz1)];

        float c00     = Mth::lerpf(d000, d100, tx);
        float c10     = Mth::lerpf(d010, d110, tx);
        float c01     = Mth::lerpf(d001, d101, tx);
        float c11     = Mth::lerpf(d011, d111, tx);
        float c0      = Mth::lerpf(c00, c01, tz);
        float c1      = Mth::lerpf(c10, c11, tz);
        float density = Mth::lerpf(c0, c1, ty);

        if (density > 0.0f)
        {
            return y;
        }
    }

    return 0;
}

Biome *TerrainGenerator::getBiomeAt(int levelX, int levelZ) const
{
    float temp     = m_temperature.GetNoise((float) levelX, (float) levelZ);
    float humidity = m_humidity.GetNoise((float) levelX, (float) levelZ);
    temp           = (temp + 1.0f) * 0.5f;
    humidity       = (humidity + 1.0f) * 0.5f;
    if (temp > 0.55f)
    {
        return Biome::byName("desert");
    }
    return Biome::byName("plains");
}

void TerrainGenerator::buildDensityGrid(float *grid, int chunkX, int chunkZ)
{
    for (int gx = 0; gx < GRID_X; gx++)
    {
        int levelX = chunkX * Chunk::SIZE_X + gx * CELL_XZ;

        double nxBase  = (double) levelX / (double) COORD_SCALE;
        double nxMain  = (double) levelX / ((double) COORD_SCALE / 80.0);
        double nxDepth = (double) levelX / (double) DEPTH_SCALE;

        for (int gz = 0; gz < GRID_Z; gz++)
        {
            int levelZ = chunkZ * Chunk::SIZE_Z + gz * CELL_XZ;

            double nzBase  = (double) levelZ / (double) COORD_SCALE;
            double nzMain  = (double) levelZ / ((double) COORD_SCALE / 80.0);
            double nzDepth = (double) levelZ / (double) DEPTH_SCALE;

            float depthRaw = m_depthNoise.GetNoise((float) nxDepth, (float) nzDepth);
            depthRaw       = Mth::clamp(depthRaw, -1.0f, 1.0f);

            for (int gy = 0; gy < GRID_Y; gy++)
            {
                double levelY = (double) (gy * CELL_Y);

                double nyBase = levelY / (double) HEIGHT_SCALE;
                double nyMain = levelY / ((double) HEIGHT_SCALE / 160.0);

                float lower =
                        m_lowerNoise.GetNoise((float) nxBase, (float) nyBase, (float) nzBase) *
                        512.0f;
                float upper =
                        m_upperNoise.GetNoise((float) nxBase, (float) nyBase, (float) nzBase) *
                        512.0f;
                float main = (m_mainNoise.GetNoise((float) nxMain, (float) nyMain, (float) nzMain) +
                              1.0f) *
                             0.5f;
                main = Mth::clamp(main, 0.0f, 1.0f);

                float density = Mth::lerpf(lower, upper, main);

                float yBias = (BASE_SIZE - (float) gy) / STRETCH_Y;
                yBias += depthRaw;

                density = density / 512.0f + yBias;

                grid[getGridIndex(gx, gy, gz)] = density;
            }
        }
    }
}

void TerrainGenerator::carveEllipsoid(Chunk &chunk, const ChunkPos &chunkPos, double centerX,
                                      double centerY, double centerZ, double radiusHorizontal,
                                      double radiusVertical)
{
    int minX = (int) floor(centerX - radiusHorizontal) - chunkPos.x * Chunk::SIZE_X;
    int maxX = (int) floor(centerX + radiusHorizontal) - chunkPos.x * Chunk::SIZE_X;
    int minZ = (int) floor(centerZ - radiusHorizontal) - chunkPos.z * Chunk::SIZE_Z;
    int maxZ = (int) floor(centerZ + radiusHorizontal) - chunkPos.z * Chunk::SIZE_Z;
    int minY = (int) floor(centerY - radiusVertical);
    int maxY = (int) floor(centerY + radiusVertical);

    minX = Mth::clamp(minX, 0, Chunk::SIZE_X - 1);
    maxX = Mth::clamp(maxX, 0, Chunk::SIZE_X - 1);
    minZ = Mth::clamp(minZ, 0, Chunk::SIZE_Z - 1);
    maxZ = Mth::clamp(maxZ, 0, Chunk::SIZE_Z - 1);
    minY = Mth::clamp(minY, 1, Chunk::SIZE_Y - 2);
    maxY = Mth::clamp(maxY, 1, Chunk::SIZE_Y - 2);

    double invRadiusHorizontal = 1.0 / radiusHorizontal;
    double invRadiusVertical   = 1.0 / radiusVertical;

    Block *bedrock = Block::byName("bedrock");
    Block *air     = Block::byName("air");

    for (int localX = minX; localX <= maxX; localX++)
    {
        double dx = ((double) (chunkPos.x * Chunk::SIZE_X + localX) + 0.5 - centerX) *
                    invRadiusHorizontal;
        double dx2 = dx * dx;
        if (dx2 >= 1.0)
        {
            continue;
        }

        for (int localZ = minZ; localZ <= maxZ; localZ++)
        {
            double dz = ((double) (chunkPos.z * Chunk::SIZE_Z + localZ) + 0.5 - centerZ) *
                        invRadiusHorizontal;
            double dz2 = dz * dz;
            if (dx2 + dz2 >= 1.0)
            {
                continue;
            }

            for (int localY = minY; localY <= maxY; localY++)
            {
                double dy   = ((double) localY + 0.5 - centerY) * invRadiusVertical;
                double dist = dx2 + dz2 + dy * dy;
                if (dist >= 1.0)
                {
                    continue;
                }

                int currentId = chunk.getBlockId(localX, localY, localZ);
                if (Block::byId(currentId) == bedrock)
                {
                    continue;
                }

                chunk.setBlock(localX, localY, localZ, air);
            }
        }
    }
}

void TerrainGenerator::carveCaveTunnel(Chunk &chunk, Random &caveRandom, const ChunkPos &chunkPos,
                                       double startX, double startY, double startZ, float radius,
                                       float yaw, float pitch, int stepCount)
{
    double positionX = startX;
    double positionY = startY;
    double positionZ = startZ;

    float yawChange   = 0.0f;
    float pitchChange = 0.0f;

    int roomStep = -1;
    if (getRandomIndex(caveRandom, 6) == 0)
        roomStep = getRandomIndex(caveRandom, stepCount / 2) + stepCount / 4;

    for (int step = 0; step < stepCount; step++)
    {
        double progress    = (double) step / (double) stepCount;
        double radiusScale = 1.5 + sin(progress * M_PI) * (double) radius;

        double radiusHorizontal = radiusScale;
        double radiusVertical   = radiusScale * 0.9;

        if (step == roomStep)
        {
            radiusHorizontal *= 2.5;
            radiusVertical *= 1.7;
        }

        carveEllipsoid(chunk, chunkPos, positionX, positionY, positionZ, radiusHorizontal,
                       radiusVertical);

        float cosPitch = cos(pitch);
        positionX += (double) (cos(yaw) * cosPitch);
        positionZ += (double) (sin(yaw) * cosPitch);
        positionY += (double) (sin(pitch));

        pitch *= 0.92f;
        pitch += pitchChange * 0.1f;
        yaw += yawChange * 0.1f;

        pitchChange = pitchChange * 0.75f + (caveRandom.nextFloat() - caveRandom.nextFloat()) *
                                                    caveRandom.nextFloat() * 2.5f;
        yawChange = yawChange * 0.9f + (caveRandom.nextFloat() - caveRandom.nextFloat()) *
                                               caveRandom.nextFloat() * 5.0f;

        if (positionY < 2.0)
        {
            break;
        }
        if (positionY > (double) (Chunk::SIZE_Y - 2))
        {
            break;
        }

        double chunkLevelMinX = (double) (chunkPos.x * Chunk::SIZE_X);
        double chunkLevelMinZ = (double) (chunkPos.z * Chunk::SIZE_Z);
        double chunkLevelMaxX = chunkLevelMinX + (double) Chunk::SIZE_X;
        double chunkLevelMaxZ = chunkLevelMinZ + (double) Chunk::SIZE_Z;

        if (positionX < chunkLevelMinX - 48.0)
        {
            break;
        }
        if (positionZ < chunkLevelMinZ - 48.0)
        {
            break;
        }
        if (positionX > chunkLevelMaxX + 48.0)
        {
            break;
        }
        if (positionZ > chunkLevelMaxZ + 48.0)
        {
            break;
        }
    }
}

void TerrainGenerator::carveCavesFromSourceChunk(Chunk &chunk, const ChunkPos &targetChunkPos,
                                                 int sourceChunkX, int sourceChunkZ)
{
    Random caveRandom(seedCave(m_seed, sourceChunkX, sourceChunkZ));

    int caveSystemCount = 0;
    if (getRandomIndex(caveRandom, 10) == 0)
    {
        caveSystemCount = getRandomIndex(caveRandom, getRandomIndex(caveRandom, 6) + 1) + 1;
    }

    bool allowEntrances = (sourceChunkX == targetChunkPos.x && sourceChunkZ == targetChunkPos.z);

    for (int caveIndex = 0; caveIndex < caveSystemCount; caveIndex++)
    {
        int startLocalX = getRandomIndex(caveRandom, Chunk::SIZE_X);
        int startLocalZ = getRandomIndex(caveRandom, Chunk::SIZE_Z);

        double startX = (double) (sourceChunkX * Chunk::SIZE_X + startLocalX);
        double startZ = (double) (sourceChunkZ * Chunk::SIZE_Z + startLocalZ);

        bool entranceRoll   = (getRandomIndex(caveRandom, 10) == 0);
        bool preferEntrance = allowEntrances && entranceRoll;

        int minY = 8;
        int maxY = 64;
        if (maxY > Chunk::SIZE_Y - 8)
        {
            maxY = Chunk::SIZE_Y - 8;
        }
        if (minY > maxY)
        {
            minY = maxY;
        }

        int startY = caveRandom.nextInt(minY, maxY);
        if (preferEntrance)
        {
            int minE = 28, maxE = 52;
            if (maxE > Chunk::SIZE_Y - 8)
            {
                maxE = Chunk::SIZE_Y - 8;
            }
            if (minE > maxE)
            {
                minE = maxE;
            }

            startY = Mth::clamp(startY, minE, maxE);
        }

        int tunnelCount = getRandomIndex(caveRandom, 4) + 1;
        if (getRandomIndex(caveRandom, 4) == 0)
            tunnelCount += getRandomIndex(caveRandom, 6) + 2;

        for (int tunnelIndex = 0; tunnelIndex < tunnelCount; tunnelIndex++)
        {
            float yaw   = caveRandom.nextFloat() * 6.283185307179586f;
            float pitch = (caveRandom.nextFloat() - 0.5f) * 0.35f;

            float radius = caveRandom.nextFloat() * 2.0f + caveRandom.nextFloat() * 1.5f;
            if (preferEntrance)
            {
                radius += 0.4f;
            }

            if (getRandomIndex(caveRandom, 12) == 0)
            {
                radius *= caveRandom.nextFloat() * caveRandom.nextFloat() * 3.0f + 1.2f;
            }

            int stepCount = getRandomIndex(caveRandom, 120) + 60;
            if (preferEntrance)
            {
                stepCount += 30;
            }

            carveCaveTunnel(chunk, caveRandom, targetChunkPos, startX, (double) startY, startZ,
                            radius, yaw, pitch, stepCount);
        }
    }
}
