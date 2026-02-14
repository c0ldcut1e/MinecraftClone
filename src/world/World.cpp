#include "World.h"

#include <algorithm>
#include <cfloat>
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
    if (ChunkManager *chunkManager = Minecraft::getInstance()->getChunkManager()) {
        std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> ready;

        chunkManager->drainFinished(ready);

        while (!ready.empty()) {
            auto [pos, chunkPtr] = std::move(ready.front());
            ready.pop_front();

            if (m_chunks.find(pos) != m_chunks.end()) continue;
            m_chunks.emplace(pos, std::move(chunkPtr));

            int worldX = pos.x * Chunk::SIZE_X;
            int worldY = pos.y * Chunk::SIZE_Y;
            int worldZ = pos.z * Chunk::SIZE_Z;
            markChunkDirty(worldX, worldY, worldZ);
            markChunkDirty(worldX - Chunk::SIZE_X, worldY, worldZ);
            markChunkDirty(worldX + Chunk::SIZE_X, worldY, worldZ);
            markChunkDirty(worldX, worldY, worldZ - Chunk::SIZE_Z);
            markChunkDirty(worldX, worldY, worldZ + Chunk::SIZE_Z);
        }
    }

    while (!m_lightUpdates.empty()) {
        Vec3 pos = m_lightUpdates.front();
        m_lightUpdates.pop_front();

        std::unique_lock<std::shared_mutex> lock(m_chunkDataMutex);

        LightEngine::updateFrom(this, (int) pos.x, (int) pos.y, (int) pos.z);
    }

    if (m_lightUpdates.empty()) {
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
    std::unique_lock<std::shared_mutex> lock(m_chunkDataMutex);

    int cx = Math::floorDiv(worldX, Chunk::SIZE_X);
    int cy = Math::floorDiv(worldY, Chunk::SIZE_Y);
    int cz = Math::floorDiv(worldZ, Chunk::SIZE_Z);

    int lx = Math::floorMod(worldX, Chunk::SIZE_X);
    int lz = Math::floorMod(worldZ, Chunk::SIZE_Z);

    ChunkPos pos{cx, cy, cz};
    Chunk *chunk = getChunk(pos);
    if (!chunk) return;

    chunk->setBlock(lx, worldY, lz, block);
    m_lightUpdates.push_front(Vec3(worldX, worldY, worldZ));
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

HitResult *World::clip(const Vec3 &origin, const Vec3 &direction, float maxDistance) {
    HitResult *result = new HitResult();
    result->setMiss();

    Vec3 _direction = direction.normalize();
    if (_direction.lengthSquared() == 0.0f) return result;

    float bestEntityT = std::numeric_limits<float>::max();
    UUID bestEntityUUID;
    uint64_t bestEntityType = 0;
    bool hasEntity          = false;

    UUID localUUID = Minecraft::getInstance()->getLocalPlayer()->getUUID();

    for (const std::unique_ptr<Entity> &entity : m_entities) {
        if (entity->getUUID() == localUUID) continue;

        const AABB &box = entity->getAABB();
        const Vec3 &min = box.getMin();
        const Vec3 &max = box.getMax();

        float tMin = 0.0f;
        float tMax = maxDistance;
        bool valid = true;

        for (int axis = 0; axis < 3 && valid; axis++) {
            double o    = axis == 0 ? origin.x : (axis == 1 ? origin.y : origin.z);
            double d    = axis == 0 ? _direction.x : (axis == 1 ? _direction.y : _direction.z);
            double bMin = axis == 0 ? min.x : (axis == 1 ? min.y : min.z);
            double bMax = axis == 0 ? max.x : (axis == 1 ? max.y : max.z);

            if (d != 0.0) {
                float inv = (float) (1.0 / d);
                float t0  = (float) ((bMin - o) * inv);
                float t1  = (float) ((bMax - o) * inv);
                if (t0 > t1) std::swap(t0, t1);
                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);
                if (tMax < tMin) valid = false;
            } else {
                if (o < bMin || o > bMax) valid = false;
            }
        }

        if (valid && tMin >= 0.0f && tMin < bestEntityT) {
            bestEntityT    = tMin;
            bestEntityUUID = entity->getUUID();
            bestEntityType = entity->getType();
            hasEntity      = true;
        }
    }

    int x = (int) floor(origin.x);
    int y = (int) floor(origin.y);
    int z = (int) floor(origin.z);

    int stepX = _direction.x > 0.0f ? 1 : -1;
    int stepY = _direction.y > 0.0f ? 1 : -1;
    int stepZ = _direction.z > 0.0f ? 1 : -1;

    float tDeltaX = _direction.x != 0.0f ? (float) fabs(1.0 / _direction.x) : std::numeric_limits<float>::max();
    float tDeltaY = _direction.y != 0.0f ? (float) fabs(1.0 / _direction.y) : std::numeric_limits<float>::max();
    float tDeltaZ = _direction.z != 0.0f ? (float) fabs(1.0 / _direction.z) : std::numeric_limits<float>::max();

    double nextBoundaryX = stepX > 0 ? (double) x + 1.0 : (double) x;
    double nextBoundaryY = stepY > 0 ? (double) y + 1.0 : (double) y;
    double nextBoundaryZ = stepZ > 0 ? (double) z + 1.0 : (double) z;

    float tMaxX = _direction.x != 0.0f ? (float) ((nextBoundaryX - origin.x) / _direction.x) : std::numeric_limits<float>::max();
    float tMaxY = _direction.y != 0.0f ? (float) ((nextBoundaryY - origin.y) / _direction.y) : std::numeric_limits<float>::max();
    float tMaxZ = _direction.z != 0.0f ? (float) ((nextBoundaryZ - origin.z) / _direction.z) : std::numeric_limits<float>::max();

    if (tMaxX < 0.0f) tMaxX = 0.0f;
    if (tMaxY < 0.0f) tMaxY = 0.0f;
    if (tMaxZ < 0.0f) tMaxZ = 0.0f;

    float bestBlockT  = std::numeric_limits<float>::max();
    int bestBlockX    = 0;
    int bestBlockY    = 0;
    int bestBlockZ    = 0;
    int bestBlockFace = -1;
    bool hasBlock     = false;

    float t  = 0.0f;
    int face = -1;

    while (t <= maxDistance) {
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                face = stepX > 0 ? 0 : 1;
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? 4 : 5;
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                face = stepY > 0 ? 2 : 3;
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? 4 : 5;
            }
        }

        if (t > maxDistance) break;

        int cx = Math::floorDiv(x, Chunk::SIZE_X);
        int cy = Math::floorDiv(y, Chunk::SIZE_Y);
        int cz = Math::floorDiv(z, Chunk::SIZE_Z);

        int lx = Math::floorMod(x, Chunk::SIZE_X);
        int lz = Math::floorMod(z, Chunk::SIZE_Z);

        Chunk *chunk = getChunk({cx, cy, cz});
        if (!chunk) continue;
        if (y < 0 || y >= Chunk::SIZE_Y) continue;

        Block *block = Block::byId((int) chunk->getBlockId(lx, y, lz));
        if (block && block->isSolid()) {
            bestBlockT    = t;
            bestBlockX    = x;
            bestBlockY    = y;
            bestBlockZ    = z;
            bestBlockFace = face;
            hasBlock      = true;
            break;
        }
    }

    if (hasEntity && (!hasBlock || bestEntityT <= bestBlockT)) {
        Vec3 hitPos = origin.add(_direction.scale(bestEntityT));
        result->setEntity(hitPos, bestEntityUUID, bestEntityType, bestEntityT);
        return result;
    }

    if (hasBlock) {
        Vec3 hitPos = origin.add(_direction.scale(bestBlockT));
        result->setBlock(hitPos, bestBlockX, bestBlockY, bestBlockZ, bestBlockFace, bestBlockT);
        return result;
    }

    return result;
}

const Vec3 &World::getSunPosition() const { return m_sunPosition; }

void World::setSunPosition(const Vec3 &position) { m_sunPosition = position; }

void World::setRenderDistance(int distance) {
    if (distance < 1) distance = 1;
    if (distance > 32) distance = 32;
    m_renderDistance = distance;
}

int World::getRenderDistance() const { return m_renderDistance; }

std::shared_mutex &World::getChunkDataMutex() { return m_chunkDataMutex; }
