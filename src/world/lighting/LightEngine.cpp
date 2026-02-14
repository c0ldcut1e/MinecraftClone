#include "LightEngine.h"

#include <queue>
#include <unordered_set>

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

void LightEngine::rebuildChunk(World *world, const ChunkPos &pos) {
    if (!world) return;

    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    for (int y = 0; y < Chunk::SIZE_Y; y++) {
        for (int z = 0; z < Chunk::SIZE_Z; z++) {
            for (int x = 0; x < Chunk::SIZE_X; x++) {
                chunk->setBlockLight(x, y, z, 0, 0, 0);
                chunk->setSkyLight(x, y, z, 0);
            }
        }
    }

    propagateSkyLight(world, pos);
    propagateBlockLight(world, pos);
}

void LightEngine::propagateSkyLight(World *world, const ChunkPos &pos) {
    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    std::queue<SkyLightNode> lightQueue;

    for (int z = 0; z < Chunk::SIZE_Z; z++) {
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            int wx = pos.x * Chunk::SIZE_X + x;
            int wz = pos.z * Chunk::SIZE_Z + z;

            for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
                Block *block = Block::byId(chunk->getBlockId(x, y, z));

                if (block->isSolid()) break;

                setSkyLight(world, BlockPos(wx, y, wz), 15);
                lightQueue.push({wx, y, wz, 15});
            }
        }
    }

    while (!lightQueue.empty()) {
        SkyLightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t currentLevel = getSkyLight(world, BlockPos(node.x, node.y, node.z));
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = getSkyLight(world, BlockPos(nx, ny, nz));

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) {
                newLevel = 15;
            } else {
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;
            }

            if (newLevel > neighborLevel) {
                setSkyLight(world, BlockPos(nx, ny, nz), newLevel);
                lightQueue.push({nx, ny, nz, newLevel});
            }
        }
    }
}

void LightEngine::propagateBlockLight(World *world, const ChunkPos &pos) {
    Chunk *chunk = world->getChunk(pos);
    if (!chunk) return;

    std::queue<LightNode> lightQueue;

    for (int y = 0; y < Chunk::SIZE_Y; y++) {
        for (int z = 0; z < Chunk::SIZE_Z; z++) {
            for (int x = 0; x < Chunk::SIZE_X; x++) {
                Block *block     = Block::byId(chunk->getBlockId(x, y, z));
                uint8_t emission = block->getLightEmission();

                if (emission == 0) continue;

                uint8_t lr, lg, lb;
                block->getLightColor(lr, lg, lb);

                uint8_t finalR = (uint8_t) ((lr / 255.0f) * emission);
                uint8_t finalG = (uint8_t) ((lg / 255.0f) * emission);
                uint8_t finalB = (uint8_t) ((lb / 255.0f) * emission);

                int wx = pos.x * Chunk::SIZE_X + x;
                int wy = y;
                int wz = pos.z * Chunk::SIZE_Z + z;

                setBlockLight(world, BlockPos(wx, wy, wz), finalR, finalG, finalB);
                lightQueue.push({wx, wy, wz, finalR, finalG, finalB});
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t cr, cg, cb;
        getBlockLight(world, BlockPos(node.x, node.y, node.z), &cr, &cg, &cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            getBlockLight(world, BlockPos(nx, ny, nz), &nr, &ng, &nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;

            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                setBlockLight(world, BlockPos(nx, ny, nz), finalR, finalG, finalB);
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

    std::unordered_set<ChunkPos, ChunkPosHash> dirtyChunks;

    std::queue<LightRemovalNode> skyRemovalQueue;
    std::queue<SkyLightNode> skyAddQueue;
    std::queue<ColoredLightRemovalNode> blockRemovalQueue;
    std::queue<LightNode> blockAddQueue;

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos baseChunkPos(cx, cy, cz);
    dirtyChunks.insert(baseChunkPos);

    Block *changedBlock = nullptr;
    {
        Chunk *chunk = world->getChunk(baseChunkPos);
        if (chunk) {
            int lx       = Math::floorMod(worldPos.x, Chunk::SIZE_X);
            int lz       = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
            changedBlock = Block::byId(chunk->getBlockId(lx, worldPos.y, lz));
        }
    }

    for (int y = worldPos.y; y < Chunk::SIZE_Y; y++) {
        BlockPos p(worldPos.x, y, worldPos.z);

        uint8_t oldLevel = getSkyLight(world, p);
        if (oldLevel > 0) {
            setSkyLight(world, p, 0);
            skyRemovalQueue.push({p.x, p.y, p.z, oldLevel});
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

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert(neighborChunkPos); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            BlockPos np(nx, ny, nz);

            uint8_t neighborLevel = getSkyLight(world, np);
            if (neighborLevel == 0) continue;

            uint8_t expectedFromThis;
            if (DIRECTIONS[i][1] == -1 && node.value == 15) {
                expectedFromThis = 15;
            } else {
                expectedFromThis = node.value > 1 ? node.value - 1 : 0;
            }

            if (neighborLevel <= expectedFromThis) {
                setSkyLight(world, np, 0);
                skyRemovalQueue.push({np.x, np.y, np.z, neighborLevel});
            } else {
                skyAddQueue.push({np.x, np.y, np.z, neighborLevel});
            }
        }
    }

    for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
        Chunk *chunk = world->getChunk(baseChunkPos);
        if (!chunk) break;

        int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
        int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);

        Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
        if (block->isSolid()) break;

        BlockPos p(worldPos.x, y, worldPos.z);

        uint8_t currentLevel = getSkyLight(world, p);
        if (currentLevel < 15) {
            setSkyLight(world, p, 15);
            skyAddQueue.push({p.x, p.y, p.z, 15});
        }
    }

    for (int i = 0; i < 6; i++) {
        int nx = worldPos.x + DIRECTIONS[i][0];
        int ny = worldPos.y + DIRECTIONS[i][1];
        int nz = worldPos.z + DIRECTIONS[i][2];

        if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

        int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
        int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
        int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

        ChunkPos neighborChunkPos(ncx, ncy, ncz);
        Chunk *neighborChunk = world->getChunk(neighborChunkPos);
        if (!neighborChunk) continue;

        BlockPos np(nx, ny, nz);

        uint8_t neighborLevel = getSkyLight(world, np);
        if (neighborLevel > 0) skyAddQueue.push({np.x, np.y, np.z, neighborLevel});
    }

    while (!skyAddQueue.empty()) {
        SkyLightNode node = skyAddQueue.front();
        skyAddQueue.pop();

        BlockPos nodePos(node.x, node.y, node.z);

        uint8_t currentLevel = getSkyLight(world, nodePos);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = nodePos.x + DIRECTIONS[i][0];
            int ny = nodePos.y + DIRECTIONS[i][1];
            int nz = nodePos.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert(neighborChunkPos); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            BlockPos np(nx, ny, nz);

            uint8_t neighborLevel = getSkyLight(world, np);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) {
                newLevel = 15;
            } else {
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;
            }

            if (newLevel > neighborLevel) {
                setSkyLight(world, np, newLevel);
                skyAddQueue.push({np.x, np.y, np.z, newLevel});
            }
        }
    }

    {
        uint8_t oldR;
        uint8_t oldG;
        uint8_t oldB;

        getBlockLight(world, worldPos, &oldR, &oldG, &oldB);

        if (oldR > 0 || oldG > 0 || oldB > 0) {
            setBlockLight(world, worldPos, 0, 0, 0);
            blockRemovalQueue.push({worldPos.x, worldPos.y, worldPos.z, oldR, oldG, oldB});
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

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert(neighborChunkPos); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            BlockPos np(nx, ny, nz);

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;

            getBlockLight(world, np, &nr, &ng, &nb);

            if (nr == 0 && ng == 0 && nb == 0) continue;

            uint8_t expectedR = node.r > 1 ? node.r - 1 : 0;
            uint8_t expectedG = node.g > 1 ? node.g - 1 : 0;
            uint8_t expectedB = node.b > 1 ? node.b - 1 : 0;

            bool shouldRemove = false;
            if (nr > 0 && nr <= expectedR) shouldRemove = true;
            if (ng > 0 && ng <= expectedG) shouldRemove = true;
            if (nb > 0 && nb <= expectedB) shouldRemove = true;

            if (shouldRemove) {
                setBlockLight(world, np, 0, 0, 0);
                blockRemovalQueue.push({np.x, np.y, np.z, nr, ng, nb});
            } else {
                if (nr > 0 || ng > 0 || nb > 0) { blockAddQueue.push({np.x, np.y, np.z, nr, ng, nb}); }
            }
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

        setBlockLight(world, worldPos, finalR, finalG, finalB);
        blockAddQueue.push({worldPos.x, worldPos.y, worldPos.z, finalR, finalG, finalB});
    }

    if (changedBlock && !changedBlock->isSolid()) {
        for (int i = 0; i < 6; i++) {
            int nx = worldPos.x + DIRECTIONS[i][0];
            int ny = worldPos.y + DIRECTIONS[i][1];
            int nz = worldPos.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            BlockPos np(nx, ny, nz);

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;

            getBlockLight(world, np, &nr, &ng, &nb);

            if (nr || ng || nb) { blockAddQueue.push({np.x, np.y, np.z, nr, ng, nb}); }
        }
    }

    while (!blockAddQueue.empty()) {
        LightNode node = blockAddQueue.front();
        blockAddQueue.pop();

        uint8_t cr;
        uint8_t cg;
        uint8_t cb;

        getBlockLight(world, BlockPos(node.x, node.y, node.z), &cr, &cg, &cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz = Math::floorDiv(nz, Chunk::SIZE_Z);

            ChunkPos neighborChunkPos(ncx, ncy, ncz);
            Chunk *neighborChunk = world->getChunk(neighborChunkPos);
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert(neighborChunkPos); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            BlockPos np(nx, ny, nz);

            uint8_t nr;
            uint8_t ng;
            uint8_t nb;

            getBlockLight(world, np, &nr, &ng, &nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;

            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                setBlockLight(world, np, finalR, finalG, finalB);
                blockAddQueue.push({np.x, np.y, np.z, finalR, finalG, finalB});
            }
        }
    }

    for (const ChunkPos &chunkPos : dirtyChunks) {
        int wx = chunkPos.x * Chunk::SIZE_X;
        int wy = chunkPos.y * Chunk::SIZE_Y;
        int wz = chunkPos.z * Chunk::SIZE_Z;

        world->markChunkDirty(BlockPos(wx, wy, wz));
    }
}

void LightEngine::getBlockLight(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, cy, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
    int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
    chunk->getBlockLight(lx, worldPos.y, lz, *r, *g, *b);
}

void LightEngine::setBlockLight(World *world, const BlockPos &worldPos, uint8_t r, uint8_t g, uint8_t b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return;

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, cy, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return;

    int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
    int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
    chunk->setBlockLight(lx, worldPos.y, lz, r, g, b);
}

uint8_t LightEngine::getSkyLight(World *world, const BlockPos &worldPos) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return 0;

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, cy, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return 0;

    int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
    int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
    return chunk->getSkyLight(lx, worldPos.y, lz);
}

void LightEngine::setSkyLight(World *world, const BlockPos &worldPos, uint8_t level) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) return;

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, cy, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) return;

    int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
    int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
    chunk->setSkyLight(lx, worldPos.y, lz, level);
}

void LightEngine::getLightLevel(World *world, const BlockPos &worldPos, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldPos.y < 0 || worldPos.y >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx = Math::floorDiv(worldPos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldPos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldPos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos(cx, cy, cz);
    Chunk *chunk = world->getChunk(chunkPos);
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = Math::floorMod(worldPos.x, Chunk::SIZE_X);
    int lz = Math::floorMod(worldPos.z, Chunk::SIZE_Z);
    chunk->getLight(lx, worldPos.y, lz, *r, *g, *b);
}
