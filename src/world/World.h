#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "chunk/Chunk.h"
#include "chunk/ChunkPos.h"

class Entity;

class World {
public:
    World();
    ~World() = default;

    void tick();

    Chunk *getChunk(const ChunkPos &pos);
    const Chunk *getChunk(const ChunkPos &pos) const;
    Chunk &createChunk(const ChunkPos &pos);
    bool hasChunk(const ChunkPos &pos) const;
    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &getChunks() const;
    void markChunkDirty(int worldX, int worldY, int worldZ);
    void clearDirtyChunks();
    const std::deque<ChunkPos> &getDirtyChunks() const;

    void addEntity(std::unique_ptr<Entity> entity);
    void removeEntity(Entity *entity);
    void tickEntities();
    const std::vector<std::unique_ptr<Entity>> &getEntities() const;

    void setBlock(int worldX, int worldY, int worldZ, Block *block);

    int getSurfaceHeight(int worldX, int worldZ) const;
    bool intersectsBlock(const AABB &aabb) const;
    bool clip(const Vec3 &origin, const Vec3 &direction, float maxDistance, int *outX, int *outY, int *outZ, int *outFace);

    const Vec3 &getSunPosition() const;
    void setSunPosition(const Vec3 &position);

    void setRenderDistance(int distance);
    int getRenderDistance() const;

private:
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
    std::deque<ChunkPos> m_dirtyChunks;
    std::mutex m_dirtyMutex;
    std::vector<Vec3> m_lightUpdates;
    bool m_emptyChunksSolid;

    std::vector<std::unique_ptr<Entity>> m_entities;

    Vec3 m_sunPosition;

    int m_renderDistance;
};
