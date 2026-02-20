#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <BS_thread_pool.hpp>

#include "../World.h"
#include "ChunkPos.h"

class ChunkManager {
public:
    explicit ChunkManager(World *world);
    ~ChunkManager();

    void start();
    void stop();
    void update(const Vec3 &playerPosition);

    void drainFinished(std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> &out);

private:
    struct GenerationTask {
        ChunkPos pos;
        int priority;
    };

    void generateChunk(const ChunkPos &pos);
    void queueChunkGeneration(const ChunkPos &pos);
    bool isChunkInRenderDistance(const ChunkPos &pos, const ChunkPos &center) const;
    int calculatePriority(const ChunkPos &pos, const ChunkPos &center) const;

    World *m_world;

    std::unique_ptr<BS::thread_pool<>> m_pool;
    std::atomic<bool> m_running;

    std::unordered_set<ChunkPos, ChunkPosHash> m_generatingChunks;
    std::mutex m_generatingMutex;

    std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> m_finished;
    std::mutex m_finishedMutex;

    ChunkPos m_lastPlayerChunk;
};
