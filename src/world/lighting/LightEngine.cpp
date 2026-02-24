#include "LightEngine.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../core/Logger.h"
#include "../../utils/math/Math.h"
#include "../block/Block.h"
#include "../chunk/Chunk.h"

static constexpr int DIRECTIONS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

struct LightRemovalNode {
    int x;
    int y;
    int z;
    uint8_t value;
};

struct ColoredLightRemovalNode {
    int x;
    int y;
    int z;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static inline uint8_t maxComponent(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t max = r;
    if (g > max) max = g;
    if (b > max) max = b;
    return max;
}

template<typename T>
struct FastQueue {
    void push(const T &v) { m_data.push_back(v); }

    T &front() { return m_data[m_head]; }

    void pop() { m_head++; }

    bool empty() const { return m_head >= m_data.size(); }

    void reserve(size_t n) { m_data.reserve(n); }

    void clear() {
        m_data.clear();
        m_head = 0;
    }

    std::vector<T> m_data;
    size_t m_head = 0;
};

struct ChunkCache {
    std::unordered_map<ChunkPos, Chunk *, ChunkPosHash> m_map;
    World *m_world;

    explicit ChunkCache(World *world) : m_world(world) { m_map.reserve(64); }

    Chunk *get(int cx, int cy, int cz) {
        ChunkPos p(cx, cy, cz);
        auto it = m_map.find(p);
        if (it != m_map.end()) return it->second;
        Chunk *chunk = m_world->getChunk(p);
        m_map[p]     = chunk;
        return chunk;
    }

    Chunk *get(const ChunkPos &p) {
        auto it = m_map.find(p);
        if (it != m_map.end()) return it->second;
        Chunk *chunk = m_world->getChunk(p);
        m_map[p]     = chunk;
        return chunk;
    }
};

static inline int chunkCoord(int w, int size) { return Math::floorDiv(w, size); }
static inline int localCoord(int w, int size) { return Math::floorMod(w, size); }

void LightEngine::rebuildChunk(World *world, const ChunkPos &pos) {
    if (!world) return;

    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    for (int y = 0; y < Chunk::SIZE_Y; y++)
        for (int z = 0; z < Chunk::SIZE_Z; z++)
            for (int x = 0; x < Chunk::SIZE_X; x++) {
                chunk->setBlockLight(x, y, z, 0, 0, 0);
                chunk->setSkyLight(x, y, z, 0);
            }

    propagateSkyLight(world, pos);
    propagateBlockLight(world, pos);
}

void LightEngine::propagateSkyLight(World *world, const ChunkPos &pos) {
    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    ChunkCache cache(world);
    cache.m_map[pos] = chunk;

    FastQueue<SkyLightNode> lightQueue;
    lightQueue.reserve(Chunk::SIZE_X * Chunk::SIZE_Z * 16);

    for (int z = 0; z < Chunk::SIZE_Z; z++)
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            int wx = pos.x * Chunk::SIZE_X + x;
            int wz = pos.z * Chunk::SIZE_Z + z;

            for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
                Block *block = Block::byId(chunk->getBlockId(x, y, z));
                if (block->isSolid()) break;

                chunk->setSkyLight(x, y, z, 15);
                lightQueue.push({wx, y, wz, 15});
            }
        }

    while (!lightQueue.empty()) {
        SkyLightNode node = lightQueue.front();
        lightQueue.pop();

        int ncx0 = chunkCoord(node.x, Chunk::SIZE_X);
        int ncz0 = chunkCoord(node.z, Chunk::SIZE_Z);
        int lx0  = localCoord(node.x, Chunk::SIZE_X);
        int lz0  = localCoord(node.z, Chunk::SIZE_Z);

        Chunk *nodeChunk = cache.get(ncx0, 0, ncz0);
        if (!nodeChunk) continue;

        uint8_t currentLevel = nodeChunk->getSkyLight(lx0, node.y, lz0);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = chunkCoord(nx, Chunk::SIZE_X);
            int ncz = chunkCoord(nz, Chunk::SIZE_Z);

            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = neighborChunk->getSkyLight(lx, ny, lz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) newLevel = 15;
            else
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;

            if (newLevel > neighborLevel) {
                neighborChunk->setSkyLight(lx, ny, lz, newLevel);
                lightQueue.push({nx, ny, nz, newLevel});
            }
        }
    }
}

void LightEngine::propagateBlockLight(World *world, const ChunkPos &pos) {
    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    ChunkCache cache(world);
    cache.m_map[pos] = chunk;

    FastQueue<LightNode> lightQueue;
    lightQueue.reserve(Chunk::SIZE_X * Chunk::SIZE_Z * 16);

    for (int y = 0; y < Chunk::SIZE_Y; y++) {
        for (int z = 0; z < Chunk::SIZE_Z; z++) {
            for (int x = 0; x < Chunk::SIZE_X; x++) {
                Block *block     = Block::byId(chunk->getBlockId(x, y, z));
                uint8_t emission = block->getLightEmission();

                if (emission == 0) continue;

                uint8_t lr;
                uint8_t lg;
                uint8_t lb;
                block->getLightColor(lr, lg, lb);

                uint8_t finalR = (uint8_t) ((lr / 255.0f) * emission);
                uint8_t finalG = (uint8_t) ((lg / 255.0f) * emission);
                uint8_t finalB = (uint8_t) ((lb / 255.0f) * emission);

                int wx = pos.x * Chunk::SIZE_X + x;
                int wy = y;
                int wz = pos.z * Chunk::SIZE_Z + z;

                chunk->setBlockLight(x, wy, z, finalR, finalG, finalB);
                lightQueue.push({wx, wy, wz, finalR, finalG, finalB});
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        int ncx0 = chunkCoord(node.x, Chunk::SIZE_X);
        int ncz0 = chunkCoord(node.z, Chunk::SIZE_Z);
        int lx0  = localCoord(node.x, Chunk::SIZE_X);
        int lz0  = localCoord(node.z, Chunk::SIZE_Z);

        Chunk *nodeChunk = cache.get(ncx0, 0, ncz0);
        if (!nodeChunk) continue;

        uint8_t cr;
        uint8_t cg;
        uint8_t cb;
        nodeChunk->getBlockLight(lx0, node.y, lz0, cr, cg, cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = chunkCoord(nx, Chunk::SIZE_X);
            int ncz = chunkCoord(nz, Chunk::SIZE_Z);

            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;
            neighborChunk->getBlockLight(lx, ny, lz, nr, ng, nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;
            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                neighborChunk->setBlockLight(lx, ny, lz, finalR, finalG, finalB);
                lightQueue.push({nx, ny, nz, finalR, finalG, finalB});
            }
        }
    }
}

void LightEngine::rebuild(World *world) {
    if (!world) return;

    const auto &chunks = world->getChunks();
    for (const auto &[pos, chunk] : chunks) {
        if (!chunk) continue;
        ChunkPos chunkPos(pos.x, pos.y, pos.z);
        rebuildChunk(world, chunkPos);
    }
}

void LightEngine::updateFrom(World *world, const BlockPos &worldPos) {
    if (!world) return;
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return;

    ChunkCache cache(world);
    std::unordered_set<ChunkPos, ChunkPosHash> dirtyChunks;

    FastQueue<LightRemovalNode> skyRemovalQueue;
    FastQueue<SkyLightNode> skyAddQueue;
    FastQueue<ColoredLightRemovalNode> blockRemovalQueue;
    FastQueue<LightNode> blockAddQueue;

    skyRemovalQueue.reserve(512);
    skyAddQueue.reserve(512);
    blockRemovalQueue.reserve(512);
    blockAddQueue.reserve(512);

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);
    ChunkPos baseChunkPos(cx, 0, cz);
    dirtyChunks.insert(baseChunkPos);

    Block *changedBlock = nullptr;

    {
        Chunk *chunk = cache.get(baseChunkPos);
        if (chunk) {
            int lx       = localCoord(worldPos.x, Chunk::SIZE_X);
            int lz       = localCoord(worldPos.z, Chunk::SIZE_Z);
            changedBlock = Block::byId(chunk->getBlockId(lx, worldPos.y, lz));
        }
    }

    {
        Chunk *chunk = cache.get(baseChunkPos);
        if (chunk) {
            int lx = localCoord(worldPos.x, Chunk::SIZE_X);
            int lz = localCoord(worldPos.z, Chunk::SIZE_Z);

            for (int y = worldPos.y; y < Chunk::SIZE_Y; y++) {
                uint8_t oldLevel = chunk->getSkyLight(lx, y, lz);
                if (oldLevel > 0) {
                    chunk->setSkyLight(lx, y, lz, 0);
                    skyRemovalQueue.push({worldPos.x, y, worldPos.z, oldLevel});
                }
            }
        }
    }

    while (!skyRemovalQueue.empty()) {
        LightRemovalNode node = skyRemovalQueue.front();
        skyRemovalQueue.pop();

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];
            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = chunkCoord(nx, Chunk::SIZE_X);
            int ncz = chunkCoord(nz, Chunk::SIZE_Z);

            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) dirtyChunks.insert(ChunkPos(ncx, 0, ncz));

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = neighborChunk->getSkyLight(lx, ny, lz);
            if (neighborLevel == 0) continue;

            uint8_t expectedFromThis;
            if (DIRECTIONS[i][1] == -1 && node.value == 15) expectedFromThis = 15;
            else
                expectedFromThis = node.value > 1 ? node.value - 1 : 0;

            if (neighborLevel <= expectedFromThis) {
                neighborChunk->setSkyLight(lx, ny, lz, 0);
                skyRemovalQueue.push({nx, ny, nz, neighborLevel});
            } else
                skyAddQueue.push({nx, ny, nz, neighborLevel});
        }
    }

    {
        Chunk *chunk = cache.get(baseChunkPos);
        if (chunk) {
            int lx = localCoord(worldPos.x, Chunk::SIZE_X);
            int lz = localCoord(worldPos.z, Chunk::SIZE_Z);

            for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
                Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
                if (block->isSolid()) break;

                uint8_t currentLevel = chunk->getSkyLight(lx, y, lz);
                if (currentLevel < 15) {
                    chunk->setSkyLight(lx, y, lz, 15);
                    skyAddQueue.push({worldPos.x, y, worldPos.z, 15});
                }
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        int nx = worldPos.x + DIRECTIONS[i][0];
        int ny = worldPos.y + DIRECTIONS[i][1];
        int nz = worldPos.z + DIRECTIONS[i][2];

        if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

        int ncx = chunkCoord(nx, Chunk::SIZE_X);
        int ncz = chunkCoord(nz, Chunk::SIZE_Z);

        Chunk *neighborChunk = cache.get(ncx, 0, ncz);
        if (!neighborChunk) continue;

        int lx = localCoord(nx, Chunk::SIZE_X);
        int lz = localCoord(nz, Chunk::SIZE_Z);

        uint8_t neighborLevel = neighborChunk->getSkyLight(lx, ny, lz);
        if (neighborLevel > 0) skyAddQueue.push({nx, ny, nz, neighborLevel});
    }

    while (!skyAddQueue.empty()) {
        SkyLightNode node = skyAddQueue.front();
        skyAddQueue.pop();

        int ncx0 = chunkCoord(node.x, Chunk::SIZE_X);
        int ncz0 = chunkCoord(node.z, Chunk::SIZE_Z);
        int lx0  = localCoord(node.x, Chunk::SIZE_X);
        int lz0  = localCoord(node.z, Chunk::SIZE_Z);

        Chunk *nodeChunk = cache.get(ncx0, 0, ncz0);
        if (!nodeChunk) continue;

        uint8_t currentLevel = nodeChunk->getSkyLight(lx0, node.y, lz0);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = chunkCoord(nx, Chunk::SIZE_X);
            int ncz = chunkCoord(nz, Chunk::SIZE_Z);

            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) dirtyChunks.insert(ChunkPos(ncx, 0, ncz));

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = neighborChunk->getSkyLight(lx, ny, lz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) newLevel = 15;
            else
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;

            if (newLevel > neighborLevel) {
                neighborChunk->setSkyLight(lx, ny, lz, newLevel);
                skyAddQueue.push({nx, ny, nz, newLevel});
            }
        }
    }

    {
        Chunk *chunk = cache.get(baseChunkPos);
        if (chunk) {
            int lx = localCoord(worldPos.x, Chunk::SIZE_X);
            int lz = localCoord(worldPos.z, Chunk::SIZE_Z);

            uint8_t oldR;
            uint8_t oldG;
            uint8_t oldB;
            chunk->getBlockLight(lx, worldPos.y, lz, oldR, oldG, oldB);

            if (oldR > 0 || oldG > 0 || oldB > 0) {
                chunk->setBlockLight(lx, worldPos.y, lz, 0, 0, 0);
                blockRemovalQueue.push({worldPos.x, worldPos.y, worldPos.z, oldR, oldG, oldB});
            }
        }
    }

    while (!blockRemovalQueue.empty()) {
        ColoredLightRemovalNode node = blockRemovalQueue.front();
        blockRemovalQueue.pop();

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];
            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = chunkCoord(nx, Chunk::SIZE_X);
            int ncz              = chunkCoord(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) dirtyChunks.insert(ChunkPos(ncx, 0, ncz));

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;
            neighborChunk->getBlockLight(lx, ny, lz, nr, ng, nb);

            if (nr == 0 && ng == 0 && nb == 0) continue;

            uint8_t expectedR = node.r > 1 ? node.r - 1 : 0;
            uint8_t expectedG = node.g > 1 ? node.g - 1 : 0;
            uint8_t expectedB = node.b > 1 ? node.b - 1 : 0;

            bool shouldRemove = false;
            if (nr > 0 && nr <= expectedR) shouldRemove = true;
            if (ng > 0 && ng <= expectedG) shouldRemove = true;
            if (nb > 0 && nb <= expectedB) shouldRemove = true;

            if (shouldRemove) {
                neighborChunk->setBlockLight(lx, ny, lz, 0, 0, 0);
                blockRemovalQueue.push({nx, ny, nz, nr, ng, nb});
            } else if (nr > 0 || ng > 0 || nb > 0)
                blockAddQueue.push({nx, ny, nz, nr, ng, nb});
        }
    }

    if (changedBlock && changedBlock->getLightEmission() > 0) {
        uint8_t emission = changedBlock->getLightEmission();
        uint8_t lr;
        uint8_t lg;
        uint8_t lb;
        changedBlock->getLightColor(lr, lg, lb);

        uint8_t finalR = (uint8_t) ((lr / 255.0f) * emission);
        uint8_t finalG = (uint8_t) ((lg / 255.0f) * emission);
        uint8_t finalB = (uint8_t) ((lb / 255.0f) * emission);

        Chunk *chunk = cache.get(baseChunkPos);
        if (chunk) {
            int lx = localCoord(worldPos.x, Chunk::SIZE_X);
            int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
            chunk->setBlockLight(lx, worldPos.y, lz, finalR, finalG, finalB);
        }

        blockAddQueue.push({worldPos.x, worldPos.y, worldPos.z, finalR, finalG, finalB});
    }

    if (changedBlock && !changedBlock->isSolid()) {
        for (int i = 0; i < 6; i++) {
            int nx = worldPos.x + DIRECTIONS[i][0];
            int ny = worldPos.y + DIRECTIONS[i][1];
            int nz = worldPos.z + DIRECTIONS[i][2];
            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = chunkCoord(nx, Chunk::SIZE_X);
            int ncz              = chunkCoord(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            int lx               = localCoord(nx, Chunk::SIZE_X);
            int lz               = localCoord(nz, Chunk::SIZE_Z);
            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            neighborChunk->getBlockLight(lx, ny, lz, nr, ng, nb);

            if (nr || ng || nb) blockAddQueue.push({nx, ny, nz, nr, ng, nb});
        }
    }

    while (!blockAddQueue.empty()) {
        LightNode node = blockAddQueue.front();
        blockAddQueue.pop();

        int ncx0 = chunkCoord(node.x, Chunk::SIZE_X);
        int ncz0 = chunkCoord(node.z, Chunk::SIZE_Z);
        int lx0  = localCoord(node.x, Chunk::SIZE_X);
        int lz0  = localCoord(node.z, Chunk::SIZE_Z);

        Chunk *nodeChunk = cache.get(ncx0, 0, ncz0);
        if (!nodeChunk) continue;

        uint8_t cr, cg, cb;
        nodeChunk->getBlockLight(lx0, node.y, lz0, cr, cg, cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];
            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = chunkCoord(nx, Chunk::SIZE_X);
            int ncz              = chunkCoord(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = cache.get(ncx, 0, ncz);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) dirtyChunks.insert(ChunkPos(ncx, 0, ncz));

            int lx = localCoord(nx, Chunk::SIZE_X);
            int lz = localCoord(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;
            neighborChunk->getBlockLight(lx, ny, lz, nr, ng, nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;
            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                neighborChunk->setBlockLight(lx, ny, lz, finalR, finalG, finalB);
                blockAddQueue.push({nx, ny, nz, finalR, finalG, finalB});
            }
        }
    }

    for (const ChunkPos &chunkPos : dirtyChunks) world->markChunkDirtyUrgent(chunkPos);
}

void LightEngine::getBlockLight(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, 0, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = localCoord(worldPos.x, Chunk::SIZE_X);
    int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
    chunk->getBlockLight(lx, worldPos.y, lz, *r, *g, *b);
}

void LightEngine::setBlockLight(World *world, const BlockPos &worldPos, uint8_t r, uint8_t g, uint8_t b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return;

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, 0, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return;

    int lx = localCoord(worldPos.x, Chunk::SIZE_X);
    int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
    chunk->setBlockLight(lx, worldPos.y, lz, r, g, b);
}

uint8_t LightEngine::getSkyLight(World *world, const BlockPos &worldPos) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return 0;

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, 0, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return 0;

    int lx = localCoord(worldPos.x, Chunk::SIZE_X);
    int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
    return chunk->getSkyLight(lx, worldPos.y, lz);
}

void LightEngine::setSkyLight(World *world, const BlockPos &worldPos, uint8_t level) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return;

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, 0, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return;

    int lx = localCoord(worldPos.x, Chunk::SIZE_X);
    int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
    chunk->setSkyLight(lx, worldPos.y, lz, level);
}

void LightEngine::getLightLevel(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx = chunkCoord(worldPos.x, Chunk::SIZE_X);
    int cz = chunkCoord(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, 0, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = localCoord(worldPos.x, Chunk::SIZE_X);
    int lz = localCoord(worldPos.z, Chunk::SIZE_Z);
    chunk->getLight(lx, worldPos.y, lz, *r, *g, *b);
}
