#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
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

    void drainFinished(std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> &out, int max);

private:
    struct GenerationTask {
        ChunkPos pos;
        int dist2;
        uint32_t epoch;
    };

    struct TaskCompare {
        bool operator()(const GenerationTask &a, const GenerationTask &b) const {
            if (a.dist2 != b.dist2) return a.dist2 > b.dist2;
            if (a.pos.x != b.pos.x) return a.pos.x > b.pos.x;
            return a.pos.z > b.pos.z;
        }
    };

    void generateChunk(const ChunkPos &pos);
    void queueChunkGeneration(const ChunkPos &pos);
    bool isChunkInRenderDistance(const ChunkPos &pos, const ChunkPos &center) const;
    int calculatePriority(const ChunkPos &pos, const ChunkPos &center) const;

    void rebuildPending(const ChunkPos &center, const std::unordered_set<ChunkPos, ChunkPosHash> &known);
    void dispatchPending();

    bool shouldStartTask(const ChunkPos &pos, uint32_t taskEpoch) const;
    bool shouldKeepResult(const ChunkPos &pos) const;

    World *m_world;

    std::unique_ptr<BS::thread_pool<>> m_pool;
    std::atomic<bool> m_running;

    std::priority_queue<GenerationTask, std::vector<GenerationTask>, TaskCompare> m_pending;
    std::unordered_set<ChunkPos, ChunkPosHash> m_pendingSet;
    std::mutex m_pendingMutex;

    std::unordered_set<ChunkPos, ChunkPosHash> m_activeSet;
    std::mutex m_activeMutex;

    std::atomic<int> m_active;
    int m_maxActive;

    std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> m_finished;
    std::mutex m_finishedMutex;

    ChunkPos m_lastPlayerChunk;

    std::atomic<int> m_centerX;
    std::atomic<int> m_centerZ;
    std::atomic<uint32_t> m_epoch;

    std::atomic<int> m_renderDistance;
};
