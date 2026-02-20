#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "../utils/HitResult.h"
#include "block/BlockPos.h"
#include "chunk/Chunk.h"
#include "chunk/ChunkPos.h"
#include "environment/Fog.h"

class Entity;

class World {
public:
    World();
    ~World() = default;

    void update(float partialTicks);
    void tick();

    Chunk *getChunk(const ChunkPos &pos);
    const Chunk *getChunk(const ChunkPos &pos) const;
    Chunk &createChunk(const ChunkPos &pos);
    bool hasChunk(const ChunkPos &pos) const;
    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &getChunks() const;
    void markChunkDirty(const BlockPos &pos);
    void markChunkDirtyUrgent(const ChunkPos &pos);
    void clearDirtyChunks();
    const std::deque<ChunkPos> &getDirtyChunks() const;

    void addEntity(std::unique_ptr<Entity> entity);
    void removeEntity(Entity *entity);
    void tickEntities();
    const std::vector<std::unique_ptr<Entity>> &getEntities() const;

    void setBlock(const BlockPos &pos, Block *block);

    int getSurfaceHeight(int worldX, int worldZ) const;
    bool intersectsBlock(const AABB &aabb) const;
    HitResult *clip(const Vec3 &origin, const Vec3 &direction, float maxDistance);

    const Vec3 &getSunPosition() const;
    void setSunPosition(const Vec3 &position);

    void setRenderDistance(int distance);
    int getRenderDistance() const;

    uint8_t getLightLevel(const BlockPos &pos) const;
    uint8_t getSkyLightLevel(const BlockPos &pos) const;
    uint8_t getBlockLightLevel(const BlockPos &pos) const;

    Fog &getFog();
    const Fog &getFog() const;

private:
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
    std::deque<ChunkPos> m_dirtyChunks;
    std::deque<ChunkPos> m_urgentDirtyChunks;
    std::mutex m_dirtyMutex;
    std::deque<BlockPos> m_lightUpdates;
    bool m_emptyChunksSolid;

    std::vector<std::unique_ptr<Entity>> m_entities;

    Vec3 m_sunPosition;

    int m_renderDistance;

    Fog m_fog;
};
