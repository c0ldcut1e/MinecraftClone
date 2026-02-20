#include "World.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../entity/Entity.h"
#include "../entity/TestEntity.h"
#include "../utils/math/Math.h"
#include "WorldRenderer.h"
#include "block/Block.h"
#include "lighting/LightEngine.h"
#include "models/ModelRegistry.h"

World::World() : m_emptyChunksSolid(true), m_sunPosition(0.0, 1000.0, 0.0), m_renderDistance(12) {}

void World::update(float partialTicks) {
    if (ChunkManager *chunkManager = Minecraft::getInstance()->getChunkManager()) {
        std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> ready;
        chunkManager->drainFinished(ready, 2);

        const int CHUNK_INTAKE_BUDGET = 2;
        int intook                    = 0;

        while (!ready.empty() && intook < CHUNK_INTAKE_BUDGET) {
            intook++;
            auto [pos, chunkPtr] = std::move(ready.front());
            ready.pop_front();

            if (m_chunks.find(pos) != m_chunks.end()) continue;
            m_chunks.emplace(pos, std::move(chunkPtr));

            BlockPos blockPos(pos.x * Chunk::SIZE_X, pos.y * Chunk::SIZE_Y, pos.z * Chunk::SIZE_Z);
            markChunkDirty(blockPos);
            if (hasChunk(ChunkPos(pos.x - 1, pos.y, pos.z))) markChunkDirty(BlockPos(blockPos.x - Chunk::SIZE_X, blockPos.y, blockPos.z));
            if (hasChunk(ChunkPos(pos.x + 1, pos.y, pos.z))) markChunkDirty(BlockPos(blockPos.x + Chunk::SIZE_X, blockPos.y, blockPos.z));
            if (hasChunk(ChunkPos(pos.x, pos.y, pos.z - 1))) markChunkDirty(BlockPos(blockPos.x, blockPos.y, blockPos.z - Chunk::SIZE_Z));
            if (hasChunk(ChunkPos(pos.x, pos.y, pos.z + 1))) markChunkDirty(BlockPos(blockPos.x, blockPos.y, blockPos.z + Chunk::SIZE_Z));
        }
    }

    {
        const int LIGHT_BUDGET = 32;

        int processed = 0;
        while (processed < LIGHT_BUDGET && !m_lightUpdates.empty()) {
            BlockPos pos = m_lightUpdates.front();
            m_lightUpdates.pop_front();

            LightEngine::updateFrom(this, pos);

            processed++;
        }
    }

    {
        const int URGENT_BUDGET = 2;

        WorldRenderer *worldRenderer = Minecraft::getInstance()->getWorldRenderer();
        for (int i = 0; i < URGENT_BUDGET; i++) {
            ChunkPos pos;
            bool has = false;

            {
                std::lock_guard<std::mutex> lock(m_dirtyMutex);

                if (!m_urgentDirtyChunks.empty()) {
                    pos = m_urgentDirtyChunks.front();
                    m_urgentDirtyChunks.pop_front();
                    m_urgentDirtyChunksSet.erase(pos);
                    has = true;
                }
            }

            if (!has) break;
            if (worldRenderer) worldRenderer->rebuildChunkUrgent(pos);
        }
    }

    {
        const int MESH_BUDGET = 16;

        for (int i = 0; i < MESH_BUDGET; i++) {
            ChunkPos pos;
            bool has = false;

            {
                std::lock_guard<std::mutex> lock(m_dirtyMutex);

                if (!m_dirtyChunks.empty()) {
                    pos = m_dirtyChunks.front();
                    m_dirtyChunks.pop_front();
                    m_dirtyChunksSet.erase(pos);
                    has = true;
                }
            }

            if (!has) break;
            Minecraft::getInstance()->getWorldRenderer()->rebuildChunk(pos);
        }
    }

    for (std::unique_ptr<Entity> &entity : m_entities)
        if (entity) entity->update(partialTicks);
}

void World::tick() { tickEntities(); }

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

void World::markChunkDirty(const BlockPos &pos) {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);

    int cx = Math::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(pos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos{cx, cy, cz};
    if (m_dirtyChunksSet.insert(chunkPos).second) m_dirtyChunks.push_back(chunkPos);
}

void World::markChunkDirtyUrgent(const ChunkPos &chunkPos) {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    if (m_urgentDirtyChunksSet.insert(chunkPos).second) m_urgentDirtyChunks.push_front(chunkPos);
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
    for (std::unique_ptr<Entity> &entity : m_entities) {
        entity->storeOld();
        entity->tick();

        if (entity->getType() == TestEntity::TYPE) {
            entity->setMoveIntent(Vec3(0.0, 0.0, 0.5));
            Vec3 pos = entity->getPosition();
            if (entity->getWorld()->getBlockId(BlockPos((int) floor(pos.x), (int) floor(pos.y), (int) floor(pos.z + 1.0))) != 0) entity->queueJump();
        }
    }
}

const std::vector<std::unique_ptr<Entity>> &World::getEntities() const { return m_entities; }

uint32_t World::getBlockId(const BlockPos &pos) const {
    int cx = Math::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(pos.z, Chunk::SIZE_Z);

    int lx = Math::floorMod(pos.x, Chunk::SIZE_X);
    int ly = Math::floorMod(pos.y, Chunk::SIZE_Y);
    int lz = Math::floorMod(pos.z, Chunk::SIZE_Z);

    const Chunk *chunk = getChunk(ChunkPos{cx, cy, cz});
    if (!chunk) return 0;
    if (ly < 0 || ly >= Chunk::SIZE_Y) return 0;

    return chunk->getBlockId(lx, ly, lz);
}

void World::setBlock(const BlockPos &pos, Block *block) {
    int cx = Math::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Math::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv(pos.z, Chunk::SIZE_Z);

    int lx = Math::floorMod(pos.x, Chunk::SIZE_X);
    int ly = Math::floorMod(pos.y, Chunk::SIZE_Y);
    int lz = Math::floorMod(pos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos{cx, cy, cz};

    {
        Chunk *chunk = getChunk(chunkPos);
        if (!chunk) return;

        chunk->setBlock(lx, ly, lz, block);
    }

    markChunkDirty(BlockPos(pos.x, pos.y, pos.z));
    m_lightUpdates.push_back(pos);
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

    BlockPos pos((int) floor(origin.x), (int) floor(origin.y), (int) floor(origin.z));

    int stepX = _direction.x > 0.0f ? 1 : -1;
    int stepY = _direction.y > 0.0f ? 1 : -1;
    int stepZ = _direction.z > 0.0f ? 1 : -1;

    float tDeltaX = _direction.x != 0.0f ? (float) fabs(1.0 / _direction.x) : std::numeric_limits<float>::max();
    float tDeltaY = _direction.y != 0.0f ? (float) fabs(1.0 / _direction.y) : std::numeric_limits<float>::max();
    float tDeltaZ = _direction.z != 0.0f ? (float) fabs(1.0 / _direction.z) : std::numeric_limits<float>::max();

    double nextBoundaryX = stepX > 0 ? (double) pos.x + 1.0 : (double) pos.x;
    double nextBoundaryY = stepY > 0 ? (double) pos.y + 1.0 : (double) pos.y;
    double nextBoundaryZ = stepZ > 0 ? (double) pos.z + 1.0 : (double) pos.z;

    float tMaxX = _direction.x != 0.0f ? (float) ((nextBoundaryX - origin.x) / _direction.x) : std::numeric_limits<float>::max();
    float tMaxY = _direction.y != 0.0f ? (float) ((nextBoundaryY - origin.y) / _direction.y) : std::numeric_limits<float>::max();
    float tMaxZ = _direction.z != 0.0f ? (float) ((nextBoundaryZ - origin.z) / _direction.z) : std::numeric_limits<float>::max();

    if (tMaxX < 0.0f) tMaxX = 0.0f;
    if (tMaxY < 0.0f) tMaxY = 0.0f;
    if (tMaxZ < 0.0f) tMaxZ = 0.0f;

    float bestBlockT = std::numeric_limits<float>::max();
    BlockPos bestBlockPos;
    Direction *bestBlockFace = nullptr;
    bool hasBlock            = false;

    float t = 0.0f;

    while (t <= maxDistance) {
        Direction *face = nullptr;

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                pos.x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                face = stepX > 0 ? Direction::WEST : Direction::EAST;
            } else {
                pos.z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? Direction::NORTH : Direction::SOUTH;
            }
        } else {
            if (tMaxY < tMaxZ) {
                pos.y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                face = stepY > 0 ? Direction::DOWN : Direction::UP;
            } else {
                pos.z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? Direction::NORTH : Direction::SOUTH;
            }
        }

        if (t > maxDistance) break;

        ChunkPos chunkPos(Math::floorDiv(pos.x, Chunk::SIZE_X), Math::floorDiv(pos.y, Chunk::SIZE_Y), Math::floorDiv(pos.z, Chunk::SIZE_Z));

        int lx = Math::floorMod(pos.x, Chunk::SIZE_X);
        int lz = Math::floorMod(pos.z, Chunk::SIZE_Z);

        Chunk *chunk = getChunk(chunkPos);
        if (!chunk) continue;
        if (pos.y < 0 || pos.y >= Chunk::SIZE_Y) continue;

        Block *block = Block::byId((int) chunk->getBlockId(lx, pos.y, lz));
        if (block && block->isSolid()) {
            bestBlockT    = t;
            bestBlockPos  = pos;
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
        result->setBlock(hitPos, bestBlockPos, bestBlockFace, bestBlockT);
        return result;
    }

    return result;
}

const Vec3 &World::getSunPosition() const { return m_sunPosition; }

void World::setSunPosition(const Vec3 &position) { m_sunPosition = position; }

void World::setRenderDistance(int distance) { m_renderDistance = distance; }

int World::getRenderDistance() const { return m_renderDistance; }

uint8_t World::getSkyLightLevel(const BlockPos &pos) const { return LightEngine::getSkyLight((World *) this, pos); }

uint8_t World::getBlockLightLevel(const BlockPos &pos) const {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    LightEngine::getBlockLight((World *) this, pos, &r, &g, &b);
    return std::max(r, std::max(g, b));
}

uint8_t World::getLightLevel(const BlockPos &pos) const {
    uint8_t sky   = getSkyLightLevel(pos);
    uint8_t block = getBlockLightLevel(pos);
    return std::max(sky, block);
}

Fog &World::getFog() { return m_fog; }

const Fog &World::getFog() const { return m_fog; }
