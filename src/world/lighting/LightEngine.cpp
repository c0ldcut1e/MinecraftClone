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
    uint8_t r, g, b;
};

static inline uint8_t maxComponent(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t max = r;
    if (g > max) max = g;
    if (b > max) max = b;
    return max;
}

void LightEngine::rebuildChunk(World *world, int cx, int cy, int cz) {
    if (!world) return;

    ChunkPos pos{cx, cy, cz};
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

    propagateSkyLight(world, cx, cy, cz);
    propagateBlockLight(world, cx, cy, cz);
}

void LightEngine::propagateSkyLight(World *world, int cx, int cy, int cz) {
    ChunkPos pos{cx, cy, cz};
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

                setSkyLight(world, wx, y, wz, 15);
                lightQueue.push({wx, y, wz, 15});
            }
        }
    }

    while (!lightQueue.empty()) {
        SkyLightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t currentLevel = getSkyLight(world, node.x, node.y, node.z);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = getSkyLight(world, nx, ny, nz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) {
                newLevel = 15;
            } else {
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;
            }

            if (newLevel > neighborLevel) {
                setSkyLight(world, nx, ny, nz, newLevel);
                lightQueue.push({nx, ny, nz, newLevel});
            }
        }
    }
}

void LightEngine::propagateBlockLight(World *world, int cx, int cy, int cz) {
    ChunkPos pos{cx, cy, cz};
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

                setBlockLight(world, wx, wy, wz, finalR, finalG, finalB);
                lightQueue.push({wx, wy, wz, finalR, finalG, finalB});
            }
        }
    }

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t cr, cg, cb;
        getBlockLight(world, node.x, node.y, node.z, &cr, &cg, &cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            getBlockLight(world, nx, ny, nz, &nr, &ng, &nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;

            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                setBlockLight(world, nx, ny, nz, finalR, finalG, finalB);
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
        rebuildChunk(world, pos.x, pos.y, pos.z);
    }
}

void LightEngine::updateFrom(World *world, int worldX, int worldY, int worldZ) {
    if (!world) return;
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) return;

    std::unordered_set<ChunkPos, ChunkPosHash> dirtyChunks;

    std::queue<LightRemovalNode> skyRemovalQueue;
    std::queue<SkyLightNode> skyAddQueue;
    std::queue<ColoredLightRemovalNode> blockRemovalQueue;
    std::queue<LightNode> blockAddQueue;

    int cx = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldZ, Chunk::SIZE_Z);

    dirtyChunks.insert({cx, cy, cz});

    Block *changedBlock = nullptr;
    {
        Chunk *chunk = world->getChunk({cx, cy, cz});
        if (chunk) {
            int lx       = Math::floorMod(worldX, Chunk::SIZE_X);
            int lz       = Math::floorMod(worldZ, Chunk::SIZE_Z);
            changedBlock = Block::byId(chunk->getBlockId(lx, worldY, lz));
        }
    }

    for (int y = worldY; y < Chunk::SIZE_Y; y++) {
        uint8_t oldLevel = getSkyLight(world, worldX, y, worldZ);
        if (oldLevel > 0) {
            setSkyLight(world, worldX, y, worldZ, 0);
            skyRemovalQueue.push({worldX, y, worldZ, oldLevel});
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

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert({ncx, ncy, ncz}); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = getSkyLight(world, nx, ny, nz);
            if (neighborLevel == 0) continue;

            uint8_t expectedFromThis;
            if (DIRECTIONS[i][1] == -1 && node.value == 15) {
                expectedFromThis = 15;
            } else {
                expectedFromThis = node.value > 1 ? node.value - 1 : 0;
            }

            if (neighborLevel <= expectedFromThis) {
                setSkyLight(world, nx, ny, nz, 0);
                skyRemovalQueue.push({nx, ny, nz, neighborLevel});
            } else {
                skyAddQueue.push({nx, ny, nz, neighborLevel});
            }
        }
    }

    for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
        Chunk *chunk = world->getChunk({cx, cy, cz});
        if (!chunk) break;

        int lx = Math::floorMod(worldX, Chunk::SIZE_X);
        int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);

        Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
        if (block->isSolid()) break;

        uint8_t currentLevel = getSkyLight(world, worldX, y, worldZ);
        if (currentLevel < 15) {
            setSkyLight(world, worldX, y, worldZ, 15);
            skyAddQueue.push({worldX, y, worldZ, 15});
        }
    }

    for (int i = 0; i < 6; i++) {
        int nx = worldX + DIRECTIONS[i][0];
        int ny = worldY + DIRECTIONS[i][1];
        int nz = worldZ + DIRECTIONS[i][2];

        if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

        int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
        int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
        int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
        Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
        if (!neighborChunk) continue;

        uint8_t neighborLevel = getSkyLight(world, nx, ny, nz);
        if (neighborLevel > 0) skyAddQueue.push({nx, ny, nz, neighborLevel});
    }

    while (!skyAddQueue.empty()) {
        SkyLightNode node = skyAddQueue.front();
        skyAddQueue.pop();

        uint8_t currentLevel = getSkyLight(world, node.x, node.y, node.z);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert({ncx, ncy, ncz}); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t neighborLevel = getSkyLight(world, nx, ny, nz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) {
                newLevel = 15;
            } else {
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;
            }

            if (newLevel > neighborLevel) {
                setSkyLight(world, nx, ny, nz, newLevel);
                skyAddQueue.push({nx, ny, nz, newLevel});
            }
        }
    }

    {
        uint8_t oldR, oldG, oldB;
        getBlockLight(world, worldX, worldY, worldZ, &oldR, &oldG, &oldB);

        if (oldR > 0 || oldG > 0 || oldB > 0) {
            setBlockLight(world, worldX, worldY, worldZ, 0, 0, 0);
            blockRemovalQueue.push({worldX, worldY, worldZ, oldR, oldG, oldB});
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

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert({ncx, ncy, ncz}); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            getBlockLight(world, nx, ny, nz, &nr, &ng, &nb);

            if (nr == 0 && ng == 0 && nb == 0) continue;

            uint8_t expectedR = node.r > 1 ? node.r - 1 : 0;
            uint8_t expectedG = node.g > 1 ? node.g - 1 : 0;
            uint8_t expectedB = node.b > 1 ? node.b - 1 : 0;

            bool shouldRemove = false;
            if (nr > 0 && nr <= expectedR) shouldRemove = true;
            if (ng > 0 && ng <= expectedG) shouldRemove = true;
            if (nb > 0 && nb <= expectedB) shouldRemove = true;

            if (shouldRemove) {
                setBlockLight(world, nx, ny, nz, 0, 0, 0);
                blockRemovalQueue.push({nx, ny, nz, nr, ng, nb});
            } else {
                if (nr > 0 || ng > 0 || nb > 0) { blockAddQueue.push({nx, ny, nz, nr, ng, nb}); }
            }
        }
    }

    if (changedBlock && changedBlock->getLightEmission() > 0) {
        uint8_t emission = changedBlock->getLightEmission();
        uint8_t lr, lg, lb;
        changedBlock->getLightColor(lr, lg, lb);

        uint8_t finalR = (uint8_t) ((lr / 255.0f) * emission);
        uint8_t finalG = (uint8_t) ((lg / 255.0f) * emission);
        uint8_t finalB = (uint8_t) ((lb / 255.0f) * emission);

        setBlockLight(world, worldX, worldY, worldZ, finalR, finalG, finalB);
        blockAddQueue.push({worldX, worldY, worldZ, finalR, finalG, finalB});
    }

    if (changedBlock && !changedBlock->isSolid()) {
        for (int i = 0; i < 6; i++) {
            int nx = worldX + DIRECTIONS[i][0];
            int ny = worldY + DIRECTIONS[i][1];
            int nz = worldZ + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            getBlockLight(world, nx, ny, nz, &nr, &ng, &nb);

            if (nr || ng || nb) { blockAddQueue.push({nx, ny, nz, nr, ng, nb}); }
        }
    }

    while (!blockAddQueue.empty()) {
        LightNode node = blockAddQueue.front();
        blockAddQueue.pop();

        uint8_t cr, cg, cb;
        getBlockLight(world, node.x, node.y, node.z, &cr, &cg, &cb);

        uint8_t maxCurrent = maxComponent(cr, cg, cb);
        if (maxCurrent <= 1) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (ny < 0 || ny >= Chunk::SIZE_Y) continue;

            int ncx              = Math::floorDiv(nx, Chunk::SIZE_X);
            int ncy              = Math::floorDiv(ny, Chunk::SIZE_Y);
            int ncz              = Math::floorDiv(nz, Chunk::SIZE_Z);
            Chunk *neighborChunk = world->getChunk({ncx, ncy, ncz});
            if (!neighborChunk) continue;

            if (ncx != cx || ncz != cz) { dirtyChunks.insert({ncx, ncy, ncz}); }

            int lx = Math::floorMod(nx, Chunk::SIZE_X);
            int lz = Math::floorMod(nz, Chunk::SIZE_Z);

            Block *neighborBlock = Block::byId(neighborChunk->getBlockId(lx, ny, lz));
            if (neighborBlock->isSolid()) continue;

            uint8_t nr, ng, nb;
            getBlockLight(world, nx, ny, nz, &nr, &ng, &nb);

            uint8_t newR = cr > 1 ? cr - 1 : 0;
            uint8_t newG = cg > 1 ? cg - 1 : 0;
            uint8_t newB = cb > 1 ? cb - 1 : 0;

            if (newR > nr || newG > ng || newB > nb) {
                uint8_t finalR = newR > nr ? newR : nr;
                uint8_t finalG = newG > ng ? newG : ng;
                uint8_t finalB = newB > nb ? newB : nb;

                setBlockLight(world, nx, ny, nz, finalR, finalG, finalB);
                blockAddQueue.push({nx, ny, nz, finalR, finalG, finalB});
            }
        }
    }

    for (const ChunkPos &chunkPos : dirtyChunks) {
        int wx = chunkPos.x * Chunk::SIZE_X;
        int wy = chunkPos.y * Chunk::SIZE_Y;
        int wz = chunkPos.z * Chunk::SIZE_Z;
        world->markChunkDirty(wx, wy, wz);
    }
}

void LightEngine::getBlockLight(World *world, int worldX, int worldY, int worldZ, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx       = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy       = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz       = Math::floorDiv(worldZ, Chunk::SIZE_Z);
    Chunk *chunk = world->getChunk({cx, cy, cz});
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);
    chunk->getBlockLight(lx, worldY, lz, *r, *g, *b);
}

void LightEngine::setBlockLight(World *world, int worldX, int worldY, int worldZ, uint8_t r, uint8_t g, uint8_t b) {
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) return;

    int cx       = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy       = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz       = Math::floorDiv(worldZ, Chunk::SIZE_Z);
    Chunk *chunk = world->getChunk({cx, cy, cz});
    if (!chunk) return;

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);
    chunk->setBlockLight(lx, worldY, lz, r, g, b);
}

uint8_t LightEngine::getSkyLight(World *world, int worldX, int worldY, int worldZ) {
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) return 0;

    int cx       = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy       = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz       = Math::floorDiv(worldZ, Chunk::SIZE_Z);
    Chunk *chunk = world->getChunk({cx, cy, cz});
    if (!chunk) return 0;

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);
    return chunk->getSkyLight(lx, worldY, lz);
}

void LightEngine::setSkyLight(World *world, int worldX, int worldY, int worldZ, uint8_t level) {
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) return;

    int cx       = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy       = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz       = Math::floorDiv(worldZ, Chunk::SIZE_Z);
    Chunk *chunk = world->getChunk({cx, cy, cz});
    if (!chunk) return;

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);
    chunk->setSkyLight(lx, worldY, lz, level);
}

void LightEngine::getLightLevel(World *world, int worldX, int worldY, int worldZ, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (worldY < 0 || worldY >= Chunk::SIZE_Y) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int cx       = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy       = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz       = Math::floorDiv(worldZ, Chunk::SIZE_Z);
    Chunk *chunk = world->getChunk({cx, cy, cz});
    if (!chunk) {
        *r = 0;
        *g = 0;
        *b = 0;
        return;
    }

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);
    chunk->getLight(lx, worldY, lz, *r, *g, *b);
}
