#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../World.h"
#include "ChunkPos.h"

class ChunkManager {
public:
    explicit ChunkManager(World *world);
    ~ChunkManager();

    void start();
    void stop();
    void update(const Vec3 &playerPosition);

private:
    struct GenerationTask {
        ChunkPos pos;
        int priority;

        bool operator<(const GenerationTask &other) const { return priority < other.priority; }
    };

    void workerThread();
    void generateChunk(const ChunkPos &pos);
    void queueChunkGeneration(const ChunkPos &pos, int priority);
    bool isChunkInRenderDistance(const ChunkPos &pos, const ChunkPos &center) const;
    int calculatePriority(const ChunkPos &pos, const ChunkPos &center) const;

    World *m_world;

    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running;

    std::priority_queue<GenerationTask> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    std::unordered_set<ChunkPos, ChunkPosHash> m_generatingChunks;
    std::mutex m_generatingMutex;

    ChunkPos m_lastPlayerChunk;
};
