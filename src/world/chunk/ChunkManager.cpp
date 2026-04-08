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
#include "../../threading/ThreadStorage.h"
#include "../../utils/math/Mth.h"
#include "../LevelRenderer.h"
#include "../block/Block.h"
#include "../chunk/ChunkMesher.h"
#include "../generation/TerrainGenerator.h"
#include "../lighting/LightEngine.h"

ChunkManager::ChunkManager(Level *level)
    : m_level(level), m_running(false), m_active(0), m_maxActive(0),
      m_lastPlayerChunk{INT32_MAX, INT32_MAX, INT32_MAX}, m_centerX(0), m_centerZ(0), m_epoch(0),
      m_renderDistance(0)
{}

ChunkManager::~ChunkManager() { stop(); }

void ChunkManager::start()
{
    if (m_running.load())
    {
        return;
    }
    m_running.store(true);

    int threadCount = std::max(1u, std::thread::hardware_concurrency() - 2);
    Logger::logInfo("Starting chunk generation with %d threads", threadCount);

    m_pool      = std::make_unique<ThreadPool>((size_t) threadCount);
    m_maxActive = std::max(1, threadCount * 4);

    if (m_level)
    {
        m_renderDistance.store(m_level->getRenderDistance());
    }
}

void ChunkManager::stop()
{
    if (!m_running.load())
    {
        return;
    }
    m_running.store(false);

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);

        while (!m_pending.empty())
        {
            m_pending.pop();
        }
        m_pendingSet.clear();
    }

    if (m_pool)
    {
        m_pool->wait();
        m_pool.reset();
    }

    {
        std::lock_guard<std::mutex> lock(m_activeMutex);

        m_activeSet.clear();
    }

    m_active.store(0);

    Logger::logInfo("Chunk generation stopped");
}

bool ChunkManager::shouldStartTask(const ChunkPos &pos, uint32_t taskEpoch) const
{
    if (!m_running.load())
    {
        return false;
    }
    if (taskEpoch != m_epoch.load())
    {
        return false;
    }

    ChunkPos center{m_centerX.load(), 0, m_centerZ.load()};
    if (!isChunkInRenderDistance(pos, center))
    {
        return false;
    }
    return true;
}

bool ChunkManager::shouldKeepResult(const ChunkPos &pos) const
{
    if (!m_running.load())
    {
        return false;
    }

    ChunkPos center{m_centerX.load(), 0, m_centerZ.load()};
    if (!isChunkInRenderDistance(pos, center))
    {
        return false;
    }
    return true;
}

void ChunkManager::rebuildPending(const ChunkPos &center,
                                  const std::unordered_set<ChunkPos, ChunkPosHash> &known)
{
    uint32_t epoch     = m_epoch.load();
    int renderDistance = m_renderDistance.load();
    int maxD2          = renderDistance * renderDistance;

    std::vector<GenerationTask> tasks;
    tasks.reserve((size_t) ((renderDistance * 2 + 1) * (renderDistance * 2 + 1)));

    for (int dz = -renderDistance; dz <= renderDistance; dz++)
        for (int dx = -renderDistance; dx <= renderDistance; dx++)
        {
            int d2 = dx * dx + dz * dz;
            if (d2 > maxD2)
            {
                continue;
            }

            ChunkPos pos{center.x + dx, 0, center.z + dz};
            if (known.count(pos))
            {
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(m_activeMutex);

                if (m_activeSet.find(pos) != m_activeSet.end())
                {
                    continue;
                }
            }

            tasks.push_back({pos, d2, epoch});
        }

    std::sort(tasks.begin(), tasks.end(), [](const GenerationTask &a, const GenerationTask &b) {
        if (a.dist2 != b.dist2)
        {
            return a.dist2 < b.dist2;
        }
        if (a.pos.x != b.pos.x)
        {
            return a.pos.x < b.pos.x;
        }
        return a.pos.z < b.pos.z;
    });

    std::lock_guard<std::mutex> lock(m_pendingMutex);
    while (!m_pending.empty()) m_pending.pop();
    m_pendingSet.clear();

    for (const GenerationTask &task : tasks)
    {
        if (m_pendingSet.insert(task.pos).second)
        {
            m_pending.push(task);
        }
    }
}

void ChunkManager::update(const Vec3 &playerPosition)
{
    int cx = Mth::floorDiv((int) playerPosition.x, Chunk::SIZE_X);
    int cz = Mth::floorDiv((int) playerPosition.z, Chunk::SIZE_Z);
    ChunkPos playerChunk{cx, 0, cz};

    if (m_level)
    {
        m_renderDistance.store(m_level->getRenderDistance());
    }

    if (!m_pool || !m_running.load())
    {
        m_lastPlayerChunk = playerChunk;
        return;
    }

    if (playerChunk != m_lastPlayerChunk)
    {
        m_lastPlayerChunk = playerChunk;

        m_centerX.store(playerChunk.x);
        m_centerZ.store(playerChunk.z);
        m_epoch.fetch_add(1);

        std::unordered_set<ChunkPos, ChunkPosHash> known;
        const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks =
                m_level->getChunks();
        known.reserve(chunks.size());
        for (const auto &[pos, _] : chunks)
        {
            known.insert(pos);
        }

        rebuildPending(playerChunk, known);
    }

    dispatchPending();
}

void ChunkManager::dispatchPending()
{
    if (!m_pool)
    {
        return;
    }
    if (!m_running.load())
    {
        return;
    }

    int startBudget = 8;
    while (startBudget-- > 0 && m_active.load() < m_maxActive)
    {
        GenerationTask task;

        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);

            if (m_pending.empty())
            {
                break;
            }

            task = m_pending.top();
            m_pending.pop();
            m_pendingSet.erase(task.pos);
        }

        if (!shouldStartTask(task.pos, task.epoch))
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_activeMutex);

            if (m_activeSet.find(task.pos) != m_activeSet.end())
            {
                continue;
            }
            m_activeSet.insert(task.pos);
        }

        m_active.fetch_add(1);

        m_pool->detachTask([this, pos = task.pos, epoch = task.epoch] {
            ThreadStorage::useDefaultThreadStorage();
            if (shouldStartTask(pos, epoch))
            {
                generateChunk(pos);
            }

            {
                std::lock_guard<std::mutex> lock(m_activeMutex);

                m_activeSet.erase(pos);
            }

            m_active.fetch_sub(1);
        });
    }
}

void ChunkManager::drainFinished(std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> *out,
                                 int max)
{
    std::lock_guard<std::mutex> lock(m_finishedMutex);

    int count = 0;
    while (!m_finished.empty() && count < max)
    {
        out->emplace_back(std::move(m_finished.front()));
        m_finished.pop_front();
        count++;
    }
}

size_t ChunkManager::getPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    return m_pending.size();
}

size_t ChunkManager::getActiveCount() const { return (size_t) m_active.load(); }

size_t ChunkManager::getMaxActiveCount() const { return (size_t) m_maxActive; }

size_t ChunkManager::getFinishedCount() const
{
    std::lock_guard<std::mutex> lock(m_finishedMutex);
    return m_finished.size();
}

size_t ChunkManager::getThreadCount() const
{
    if (!m_pool)
    {
        return 0;
    }
    return m_pool->getThreadCount();
}

void ChunkManager::generateChunk(const ChunkPos &pos)
{
    thread_local TerrainGenerator generator(0);

    std::unique_ptr<Chunk> chunk = std::make_unique<Chunk>(pos);

    if (m_level && m_level->isWorldBorderEnabled() && !m_level->isChunkInsideWorldBorder(pos))
    {
        Block *borderBlock = Block::byName("world_border");
        for (int z = 0; z < Chunk::SIZE_Z; z++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int x = 0; x < Chunk::SIZE_X; x++)
                {
                    chunk->setBlock(x, y, z, borderBlock);
                    chunk->setSkyLight(x, y, z, 0);
                }
            }
        }

        if (!shouldKeepResult(pos))
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_finishedMutex);

            m_finished.emplace_back(pos, std::move(chunk));
        }
        return;
    }

    generator.generateChunk(*chunk, pos);

    for (int z = 0; z < Chunk::SIZE_Z; z++)
    {
        for (int x = 0; x < Chunk::SIZE_X; x++)
        {
            {
                bool blocked = false;
                for (int y = Chunk::SIZE_Y - 1; y >= 0; y--)
                {
                    if (blocked)
                    {
                        chunk->setSkyLight(x, y, z, 0);
                        continue;
                    }

                    if (Block::byId(chunk->getBlockId(x, y, z))->isSolid())
                    {
                        blocked = true;
                        chunk->setSkyLight(x, y, z, 0);
                    }
                    else
                    {
                        chunk->setSkyLight(x, y, z, 15);
                    }
                }
            }
        }
    }

    std::queue<LightEngine::SkyLightNode> lightQueue;

    for (int y = 0; y < Chunk::SIZE_Y; y++)
    {
        for (int z = 0; z < Chunk::SIZE_Z; z++)
        {
            for (int x = 0; x < Chunk::SIZE_X; x++)
            {
                if (chunk->getSkyLight(x, y, z) == 15)
                {
                    lightQueue.push({x, y, z, 15});
                }
            }
        }
    }

    static const int DIRECTIONS[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                         {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

    while (!lightQueue.empty())
    {
        LightEngine::SkyLightNode node = lightQueue.front();
        lightQueue.pop();

        uint8_t currentLevel = chunk->getSkyLight(node.x, node.y, node.z);
        if (currentLevel == 0)
        {
            continue;
        }

        for (int i = 0; i < 6; i++)
        {
            int nx = node.x + DIRECTIONS[i][0];
            int ny = node.y + DIRECTIONS[i][1];
            int nz = node.z + DIRECTIONS[i][2];
            if (nx < 0 || nx >= Chunk::SIZE_X || ny < 0 || ny >= Chunk::SIZE_Y || nz < 0 ||
                nz >= Chunk::SIZE_Z)
            {
                continue;
            }

            if (Block::byId(chunk->getBlockId(nx, ny, nz))->isSolid())
            {
                continue;
            }

            uint8_t neighborLevel = chunk->getSkyLight(nx, ny, nz);

            uint8_t newLevel;
            if (DIRECTIONS[i][1] == -1 && currentLevel == 15)
            {
                newLevel = 15;
            }
            else
            {
                newLevel = currentLevel > 1 ? currentLevel - 1 : 0;
            }

            if (newLevel > neighborLevel)
            {
                chunk->setSkyLight(nx, ny, nz, newLevel);
                lightQueue.push({nx, ny, nz, newLevel});
            }
        }
    }

    if (!shouldKeepResult(pos))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_finishedMutex);

        m_finished.emplace_back(pos, std::move(chunk));
    }
}

void ChunkManager::queueChunkGeneration(const ChunkPos &pos)
{
    if (!m_pool)
    {
        return;
    }
    if (!m_running.load())
    {
        return;
    }

    ChunkPos center{m_centerX.load(), 0, m_centerZ.load()};
    if (!isChunkInRenderDistance(pos, center))
        return;

    {
        std::lock_guard<std::mutex> lock(m_activeMutex);

        if (m_activeSet.find(pos) != m_activeSet.end())
        {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);

        if (m_pendingSet.find(pos) != m_pendingSet.end())
        {
            return;
        }
        m_pendingSet.insert(pos);

        int dx = pos.x - center.x;
        int dz = pos.z - center.z;
        int d2 = dx * dx + dz * dz;

        m_pending.push({pos, d2, m_epoch.load()});
    }

    dispatchPending();
}

bool ChunkManager::isChunkInRenderDistance(const ChunkPos &pos, const ChunkPos &center) const
{
    int dx             = pos.x - center.x;
    int dz             = pos.z - center.z;
    int renderDistance = m_renderDistance.load();
    return (dx * dx + dz * dz) <= (renderDistance * renderDistance);
}

int ChunkManager::calculatePriority(const ChunkPos &pos, const ChunkPos &center) const
{
    int dx = pos.x - center.x;
    int dz = pos.z - center.z;
    return -(dx * dx + dz * dz);
}
