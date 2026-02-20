#include "ChunkManager.h"

#include <algorithm>
#include <cmath>
#include <queue>

#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "../../core/Logger.h"
#include "../../core/Minecraft.h"
#include "../../utils/math/Math.h"
#include "../WorldRenderer.h"
#include "../block/Block.h"
#include "../chunk/ChunkMesher.h"
#include "../generation/TerrainGenerator.h"
#include "../lighting/LightEngine.h"

ChunkManager::ChunkManager(World *world) : m_world(world), m_running(false), m_inFlight(0), m_maxInFlight(0), m_lastPlayerChunk{INT32_MAX, INT32_MAX, INT32_MAX} {}

ChunkManager::~ChunkManager() { stop(); }

void ChunkManager::start() {
    if (m_running.load()) return;
    m_running.store(true);

    int threadCount = std::max(1u, std::thread::hardware_concurrency() - 2);
    Logger::logInfo("Starting chunk generation with %d threads", threadCount);

    m_pool        = std::make_unique<BS::thread_pool<>>((size_t) threadCount);
    m_maxInFlight = std::max(1, threadCount * 4);
}

void ChunkManager::stop() {
    if (!m_running.load()) return;

    m_running.store(false);

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        while (!m_pending.empty()) m_pending.pop();
    }

    if (m_pool) {
        m_pool->wait();
        m_pool.reset();
    }

    {
        std::lock_guard<std::mutex> genLock(m_generatingMutex);
        m_generatingChunks.clear();
    }

    m_inFlight.store(0);

    Logger::logInfo("Chunk generation stopped");
}

void ChunkManager::update(const Vec3 &playerPosition) {
    int cx = Math::floorDiv((int) playerPosition.x, Chunk::SIZE_X);
    int cz = Math::floorDiv((int) playerPosition.z, Chunk::SIZE_Z);
    ChunkPos playerChunk{cx, 0, cz};

    if (playerChunk == m_lastPlayerChunk) {
        if (m_running.load() && m_pool) dispatchPending();
        return;
    }
    m_lastPlayerChunk = playerChunk;

    if (!m_pool || !m_running.load()) return;

    std::unordered_set<ChunkPos, ChunkPosHash> known;
    const auto &chunks = m_world->getChunks();
    known.reserve(chunks.size());
    for (const auto &[pos, _] : chunks) known.insert(pos);

    int renderDistance = m_world->getRenderDistance();

    m_pool->detach_task([this, playerChunk, known = std::move(known), renderDistance] {
        if (!m_running.load()) return;

        std::vector<GenerationTask> tasks;
        for (int dz = -renderDistance; dz <= renderDistance; dz++)
            for (int dx = -renderDistance; dx <= renderDistance; dx++) {
                ChunkPos pos{playerChunk.x + dx, 0, playerChunk.z + dz};
                if (!isChunkInRenderDistance(pos, playerChunk)) continue;
                if (known.count(pos)) continue;
                tasks.push_back({pos, calculatePriority(pos, playerChunk)});
            }

        std::sort(tasks.begin(), tasks.end(), [](const GenerationTask &a, const GenerationTask &b) { return a.priority > b.priority; });

        {
            std::lock_guard<std::mutex> genLock(m_generatingMutex);
            std::lock_guard<std::mutex> pendLock(m_pendingMutex);
            for (const GenerationTask &t : tasks) {
                if (m_generatingChunks.find(t.pos) != m_generatingChunks.end()) continue;
                m_generatingChunks.insert(t.pos);
                m_pending.push(t);
            }
        }

        dispatchPending();
    });
}

void ChunkManager::dispatchPending() {
    if (!m_pool) return;
    if (!m_running.load()) return;

    while (m_inFlight.load() < m_maxInFlight) {
        GenerationTask task;

        {
            std::lock_guard<std::mutex> pendLock(m_pendingMutex);
            if (m_pending.empty()) break;
            task = m_pending.top();
            m_pending.pop();
        }

        m_inFlight.fetch_add(1);

        m_pool->detach_task([this, pos = task.pos] {
            if (!m_running.load()) {
                std::lock_guard<std::mutex> genLock(m_generatingMutex);
                m_generatingChunks.erase(pos);
                m_inFlight.fetch_sub(1);
                return;
            }

            generateChunk(pos);

            {
                std::lock_guard<std::mutex> genLock(m_generatingMutex);
                m_generatingChunks.erase(pos);
            }

            m_inFlight.fetch_sub(1);

            if (m_running.load()) dispatchPending();
        });
    }
}

void ChunkManager::drainFinished(std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> &out, int max) {
    std::lock_guard<std::mutex> lock(m_finishedMutex);
    int count = 0;
    while (!m_finished.empty() && count < max) {
        out.emplace_back(std::move(m_finished.front()));
        m_finished.pop_front();
        count++;
    }
}

void ChunkManager::generateChunk(const ChunkPos &pos) {
#ifndef _WIN32
    setpriority(PRIO_PROCESS, 0, 10);
#endif

    thread_local TerrainGenerator generator(0xDEADCAFE);

    std::unique_ptr<Chunk> chunk = std::make_unique<Chunk>(pos);

    generator.generateChunk(*chunk, pos);

    for (int z = 0; z < Chunk::SIZE_Z; z++)
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            bool blocked = false;

            for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
                if (blocked) {
                    chunk->setSkyLight(x, y, z, 0);
                    continue;
                }

                if (Block::byId(chunk->getBlockId(x, y, z))->isSolid()) {
                    blocked = true;
                    chunk->setSkyLight(x, y, z, 0);
                } else
                    chunk->setSkyLight(x, y, z, 15);
            }
        }

    std::queue<LightEngine::SkyLightNode> lightQueue;

    for (int y = 0; y < Chunk::SIZE_Y; y++)
        for (int z = 0; z < Chunk::SIZE_Z; z++)
            for (int x = 0; x < Chunk::SIZE_X; x++)
                if (chunk->getSkyLight(x, y, z) == 15) lightQueue.push({x, y, z, 15});

    static const int DIRECTIONS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    while (!lightQueue.empty()) {
        LightEngine::SkyLightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t currentLevel = chunk->getSkyLight(node.x, node.y, node.z);
        if (currentLevel == 0) continue;

        for (int i = 0; i < 6; i++) {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];

            if (nx < 0 || nx >= Chunk::SIZE_X || ny < 0 || ny >= Chunk::SIZE_Y || nz < 0 || nz >= Chunk::SIZE_Z) continue;
            if (Block::byId(chunk->getBlockId(nx, ny, nz))->isSolid()) continue;

            uint8_t neighborLevel = chunk->getSkyLight(nx, ny, nz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15) newLevel = 15;
            else
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;

            if (newLevel > neighborLevel) {
                chunk->setSkyLight(nx, ny, nz, newLevel);
                lightQueue.push({nx, ny, nz, newLevel});
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_finishedMutex);
        m_finished.emplace_back(pos, std::move(chunk));
    }
}

void ChunkManager::queueChunkGeneration(const ChunkPos &pos) {
    if (!m_pool) return;
    if (!m_running.load()) return;

    {
        std::lock_guard<std::mutex> genLock(m_generatingMutex);
        if (m_generatingChunks.find(pos) != m_generatingChunks.end()) return;
        m_generatingChunks.insert(pos);

        std::lock_guard<std::mutex> pendLock(m_pendingMutex);
        m_pending.push({pos, 0});
    }

    dispatchPending();
}

bool ChunkManager::isChunkInRenderDistance(const ChunkPos &pos, const ChunkPos &center) const {
    int dx             = pos.x - center.x;
    int dz             = pos.z - center.z;
    int renderDistance = m_world->getRenderDistance();
    return (dx * dx + dz * dz) <= (renderDistance * renderDistance);
}

int ChunkManager::calculatePriority(const ChunkPos &pos, const ChunkPos &center) const {
    int dx = pos.x - center.x;
    int dz = pos.z - center.z;
    return -(dx * dx + dz * dz);
}
