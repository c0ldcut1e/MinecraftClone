#include "World.h"

#include <algorithm>
#include <cmath>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../entity/Entity.h"
#include "../utils/math/Math.h"
#include "WorldRenderer.h"
#include "block/Block.h"
#include "lighting/LightEngine.h"

World::World() : m_emptyChunksSolid(true), m_sunPosition(0.0, 1000.0, 0.0), m_renderDistance(12) {}

void World::tick() {
    while (!m_lightUpdates.empty()) {
        Vec3 pos = m_lightUpdates.front();
        m_lightUpdates.erase(m_lightUpdates.begin());
        LightEngine::updateFrom(this, (int) pos.x, (int) pos.y, (int) pos.z);
    }

    {
        std::lock_guard<std::mutex> lock(m_dirtyMutex);

        if (!m_dirtyChunks.empty()) {
            ChunkPos pos = m_dirtyChunks.front();
            m_dirtyChunks.pop_front();
            Minecraft::getInstance()->getWorldRenderer()->rebuildChunk(pos.x, pos.y, pos.z);
        }
    }

    tickEntities();
}

bool World::hasChunk(const ChunkPos &pos) const { return m_chunks.find(pos) != m_chunks.end(); }

Chunk *World::getChunk(const ChunkPos &pos) {
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) return nullptr;
    return it->second.get();
}

const Chunk *World::getChunk(const ChunkPos &pos) const {
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) return nullptr;
    return it->second.get();
}

Chunk &World::createChunk(const ChunkPos &pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) return *it->second;

    std::unique_ptr<Chunk> chunk = std::make_unique<Chunk>(pos);
    Chunk &ref                   = *chunk;

    m_chunks.emplace(pos, std::move(chunk));

    Logger::logInfo("Created chunk (%d, %d, %d)", pos.x, pos.y, pos.z);

    return ref;
}

const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &World::getChunks() const { return m_chunks; }

void World::markChunkDirty(int worldX, int worldY, int worldZ) {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);

    int cx = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldZ, Chunk::SIZE_Z);

    ChunkPos pos{cx, cy, cz};

    for (size_t i = 0; i < m_dirtyChunks.size(); i++)
        if (m_dirtyChunks[i].x == pos.x && m_dirtyChunks[i].y == pos.y && m_dirtyChunks[i].z == pos.z) return;

    m_dirtyChunks.push_back(pos);
}

const std::deque<ChunkPos> &World::getDirtyChunks() const { return m_dirtyChunks; }

void World::clearDirtyChunks() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);

    m_dirtyChunks.clear();
}

void World::addEntity(std::unique_ptr<Entity> entity) { m_entities.push_back(std::move(entity)); }

void World::removeEntity(Entity *entity) {
    m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(), [entity](const std::unique_ptr<Entity> &ptr) { return ptr.get() == entity; }), m_entities.end());
}

void World::tickEntities() {
    for (std::unique_ptr<Entity> &entity : m_entities) entity->tick();
}

const std::vector<std::unique_ptr<Entity>> &World::getEntities() const { return m_entities; }

void World::setBlock(int worldX, int worldY, int worldZ, Block *block) {
    int cx = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldZ, Chunk::SIZE_Z);

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);

    ChunkPos pos{cx, cy, cz};
    Chunk *chunk = getChunk(pos);
    if (!chunk) return;

    chunk->setBlock(lx, worldY, lz, block);

    LightEngine::updateFrom(this, worldX, worldY, worldZ);

    WorldRenderer *renderer = Minecraft::getInstance()->getWorldRenderer();
    Minecraft::getInstance()->getWorldRenderer()->rebuildChunk(cx, cy, cz);

    if (lx == 0) renderer->rebuildChunk(cx - 1, cy, cz);
    if (lx == Chunk::SIZE_X - 1) renderer->rebuildChunk(cx + 1, cy, cz);
    if (lz == 0) renderer->rebuildChunk(cx, cy, cz - 1);
    if (lz == Chunk::SIZE_Z - 1) renderer->rebuildChunk(cx, cy, cz + 1);
}

int World::getSurfaceHeight(int worldX, int worldZ) const {
    int chunkX = worldX / Chunk::SIZE_X;
    int chunkZ = worldZ / Chunk::SIZE_Z;

    int localX = worldX % Chunk::SIZE_X;
    int localZ = worldZ % Chunk::SIZE_Z;

    if (localX < 0) localX += Chunk::SIZE_X;
    if (localZ < 0) localZ += Chunk::SIZE_Z;

    ChunkPos pos{chunkX, 0, chunkZ};

    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) return 0;

    const Chunk &chunk = *it->second;

    for (int y = Chunk::SIZE_Y - 1; y >= 0; y--)
        if (Block::byId(chunk.getBlockId(localX, y, localZ))->isSolid()) return y;

    return 0;
}

bool World::intersectsBlock(const AABB &aabb) const {
    int minX = (int) floor(aabb.getMin().x);
    int minY = (int) floor(aabb.getMin().y);
    int minZ = (int) floor(aabb.getMin().z);

    int maxX = (int) floor(aabb.getMax().x - 1e-6);
    int maxY = (int) floor(aabb.getMax().y - 1e-6);
    int maxZ = (int) floor(aabb.getMax().z - 1e-6);

    for (int z = minZ; z <= maxZ; z++)
        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++) {
                int cx = Math::floorDiv(x, Chunk::SIZE_X);
                int cy = Math::floorDiv(y, Chunk::SIZE_Y);
                int cz = Math::floorDiv(z, Chunk::SIZE_Z);

                int lx = Math::floorMod(x, Chunk::SIZE_X);
                int lz = Math::floorMod(z, Chunk::SIZE_Z);

                const Chunk *chunk = getChunk({cx, cy, cz});
                if (!chunk) {
                    if (m_emptyChunksSolid) return true;
                    continue;
                }

                Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
                if (!block->isSolid()) continue;

                AABB blockBox(Vec3(x, y, z), Vec3(x + 1, y + 1, z + 1));
                if (aabb.intersects(blockBox)) return true;
            }

    return false;
}

bool World::clip(const Vec3 &origin, const Vec3 &direction, float maxDistance, int *outX, int *outY, int *outZ, int *outFace) {
    const float step = 0.05f;

    Vec3 dir = direction.normalize();
    Vec3 pos = origin;

    int lastX = (int) floor(pos.x);
    int lastY = (int) floor(pos.y);
    int lastZ = (int) floor(pos.z);

    for (float t = 0.0f; t <= maxDistance; t += step) {
        pos = origin.add(dir.scale(t));

        int x = (int) floor(pos.x);
        int y = (int) floor(pos.y);
        int z = (int) floor(pos.z);

        if (x == lastX && y == lastY && z == lastZ) continue;

        int cx = Math::floorDiv(x, Chunk::SIZE_X);
        int cy = Math::floorDiv(y, Chunk::SIZE_Y);
        int cz = Math::floorDiv(z, Chunk::SIZE_Z);

        int lx = Math::floorMod(x, Chunk::SIZE_X);
        int lz = Math::floorMod(z, Chunk::SIZE_Z);

        Chunk *chunk = getChunk({cx, cy, cz});
        if (!chunk) {
            lastX = x;
            lastY = y;
            lastZ = z;
            continue;
        }

        uint32_t id  = chunk->getBlockId(lx, y, lz);
        Block *block = Block::byId(id);

        if (block->isSolid()) {
            *outX = x;
            *outY = y;
            *outZ = z;

            int dx = x - lastX;
            int dy = y - lastY;
            int dz = z - lastZ;
            if (dx == 1) *outFace = 0;
            if (dx == -1) *outFace = 1;
            if (dy == 1) *outFace = 2;
            if (dy == -1) *outFace = 3;
            if (dz == 1) *outFace = 4;
            if (dz == -1) *outFace = 5;

            return true;
        }

        lastX = x;
        lastY = y;
        lastZ = z;
    }

    return false;
}

const Vec3 &World::getSunPosition() const { return m_sunPosition; }

void World::setSunPosition(const Vec3 &position) { m_sunPosition = position; }

void World::setRenderDistance(int distance) {
    if (distance < 1) distance = 1;
    if (distance > 32) distance = 32;
    m_renderDistance = distance;
}

int World::getRenderDistance() const { return m_renderDistance; }
