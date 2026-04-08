#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "DimensionTime.h"
#include "block/BlockPos.h"
#include "chunk/Chunk.h"
#include "chunk/ChunkPos.h"
#include "chunk/storage/ChunkCache.h"
#include "environment/Fog.h"

class Dimension
{
public:
    Dimension();

    Chunk *getChunk(const ChunkPos &pos);
    const Chunk *getChunk(const ChunkPos &pos) const;
    Chunk &createChunk(const ChunkPos &pos);
    bool adoptChunk(const ChunkPos &pos, std::unique_ptr<Chunk> chunk);
    bool hasChunk(const ChunkPos &pos) const;
    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &getChunks() const;

    void markChunkDirty(const BlockPos &pos);
    void markChunkDirtyUrgent(const ChunkPos &pos);
    bool pollDirtyChunk(ChunkPos *outPos);
    bool pollUrgentDirtyChunk(ChunkPos *outPos);
    void clearDirtyChunks();
    const std::deque<ChunkPos> &getDirtyChunks() const;
    size_t getQueuedDirtyChunkCount() const;
    size_t getUrgentDirtyChunkCount() const;

    uint32_t getBlockId(const BlockPos &pos) const;
    int getSurfaceHeight(int levelX, int levelZ) const;
    bool intersectsBlock(const AABB &aabb) const;
    void setEmptyChunksSolid(bool solid);
    bool areEmptyChunksSolid() const;

    void queueLightUpdate(const BlockPos &pos);
    bool pollLightUpdate(BlockPos *outPos);
    size_t getQueuedLightUpdateCount() const;

    void setRenderDistance(int distance);
    int getRenderDistance() const;

    Fog &getFog();
    const Fog &getFog() const;

    void advanceTime();
    const DimensionTime &getDimensionTime() const;
    void setDimensionTime(uint64_t ticks);
    float getDayFraction() const;
    float getCelestialAngle() const;
    float getDaylightFactor() const;
    uint8_t getSkyLightClamp() const;

    void setDaylightCurveTicks(int darkStartTick, int darkPeakTick);
    int getDarkStartTick() const;
    int getDarkPeakTick() const;

private:
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> m_chunks;
    mutable ChunkCache m_chunkCache;
    std::deque<ChunkPos> m_dirtyChunks;
    std::deque<ChunkPos> m_urgentDirtyChunks;
    std::unordered_set<ChunkPos, ChunkPosHash> m_dirtyChunksSet;
    std::unordered_set<ChunkPos, ChunkPosHash> m_urgentDirtyChunksSet;
    mutable std::mutex m_dirtyMutex;
    std::deque<BlockPos> m_lightUpdates;
    bool m_emptyChunksSolid;
    int m_renderDistance;
    Fog m_fog;
    DimensionTime m_dimensionTime;
};
