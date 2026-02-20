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

ChunkManager::ChunkManager(World *world) : m_world(world), m_running(false), m_lastPlayerChunk{INT32_MAX, INT32_MAX, INT32_MAX} {}

ChunkManager::~ChunkManager() { stop(); }

void ChunkManager::start() {
    if (m_running.load()) return;
    m_running.store(true);

    int threadCount = std::max(1u, std::thread::hardware_concurrency() - 2);
    threadCount /= 2;
    Logger::logInfo("Starting chunk generation with %d threads", threadCount);

    m_pool = std::make_unique<BS::thread_pool<>>((size_t) threadCount);
}

void ChunkManager::stop() {
    if (!m_running.load()) return;

    m_running.store(false);

    if (m_pool) {
        m_pool->wait();
        m_pool.reset();
    }

    Logger::logInfo("Chunk generation stopped");
}

void ChunkManager::update(const Vec3 &playerPosition) {
    int cx = Math::floorDiv((int) playerPosition.x, Chunk::SIZE_X);
    int cy = Math::floorDiv((int) playerPosition.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv((int) playerPosition.z, Chunk::SIZE_Z);
    ChunkPos playerChunk{cx, cy, cz};
    if (playerChunk == m_lastPlayerChunk) return;
    m_lastPlayerChunk = playerChunk;

    std::vector<GenerationTask> tasks;

    int renderDistance = m_world->getRenderDistance();
    for (int dz = -renderDistance; dz <= renderDistance; dz++)
        for (int dx = -renderDistance; dx <= renderDistance; dx++) {
            ChunkPos pos{cx + dx, 0, cz + dz};
            if (!isChunkInRenderDistance(pos, playerChunk)) continue;
            if (m_world->hasChunk(pos)) continue;

            int priority = calculatePriority(pos, playerChunk);

            {
                std::lock_guard<std::mutex> genLock(m_generatingMutex);
                if (m_generatingChunks.find(pos) != m_generatingChunks.end()) continue;
                m_generatingChunks.insert(pos);
            }

            tasks.push_back({pos, priority});
        }

    if (tasks.empty()) return;
    if (!m_pool) return;
    if (!m_running.load()) return;

    std::sort(tasks.begin(), tasks.end(), [](const GenerationTask &a, const GenerationTask &b) { return a.priority > b.priority; });

    for (const GenerationTask &t : tasks) {
        m_pool->detach_task([this, pos = t.pos] {
            if (!m_running.load()) {
                std::lock_guard<std::mutex> genLock(m_generatingMutex);
                m_generatingChunks.erase(pos);
                return;
            }

            generateChunk(pos);

            std::lock_guard<std::mutex> genLock(m_generatingMutex);
            m_generatingChunks.erase(pos);
        });
    }
}

void ChunkManager::drainFinished(std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> &out) {
    std::lock_guard<std::mutex> lock(m_finishedMutex);
    out.swap(m_finished);
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

    struct LightNode {
        int x, y, z;
        uint8_t level;
    };

    std::queue<LightNode> lightQueue;

    for (int y = 0; y < Chunk::SIZE_Y; y++)
        for (int z = 0; z < Chunk::SIZE_Z; z++)
            for (int x = 0; x < Chunk::SIZE_X; x++)
                if (chunk->getSkyLight(x, y, z) == 15) lightQueue.push({x, y, z, 15});

    static const int DIRECTIONS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    while (!lightQueue.empty()) {
        LightNode node = lightQueue.front();
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
        const Chunk *westChunk  = m_world->getChunk(ChunkPos(pos.x - 1, pos.y, pos.z));
        const Chunk *eastChunk  = m_world->getChunk(ChunkPos(pos.x + 1, pos.y, pos.z));
        const Chunk *northChunk = m_world->getChunk(ChunkPos(pos.x, pos.y, pos.z - 1));
        const Chunk *southChunk = m_world->getChunk(ChunkPos(pos.x, pos.y, pos.z + 1));

        for (int y = 0; y < Chunk::SIZE_Y; y++) {
            for (int z = 0; z < Chunk::SIZE_Z; z++) {
                if (westChunk && !Block::byId(chunk->getBlockId(0, y, z))->isSolid()) {
                    uint8_t neighborLight = westChunk->getSkyLight(Chunk::SIZE_X - 1, y, z);
                    if (neighborLight > 1) {
                        uint8_t propagated = neighborLight - 1;
                        if (propagated > chunk->getSkyLight(0, y, z)) {
                            chunk->setSkyLight(0, y, z, propagated);
                            lightQueue.push({0, y, z, propagated});
                        }
                    }
                }

                if (eastChunk && !Block::byId(chunk->getBlockId(Chunk::SIZE_X - 1, y, z))->isSolid()) {
                    uint8_t neighborLight = eastChunk->getSkyLight(0, y, z);
                    if (neighborLight > 1) {
                        uint8_t propagated = neighborLight - 1;
                        if (propagated > chunk->getSkyLight(Chunk::SIZE_X - 1, y, z)) {
                            chunk->setSkyLight(Chunk::SIZE_X - 1, y, z, propagated);
                            lightQueue.push({Chunk::SIZE_X - 1, y, z, propagated});
                        }
                    }
                }
            }

            for (int x = 0; x < Chunk::SIZE_X; x++) {
                if (northChunk && !Block::byId(chunk->getBlockId(x, y, 0))->isSolid()) {
                    uint8_t neighborLight = northChunk->getSkyLight(x, y, Chunk::SIZE_Z - 1);
                    if (neighborLight > 1) {
                        uint8_t propagated = neighborLight - 1;
                        if (propagated > chunk->getSkyLight(x, y, 0)) {
                            chunk->setSkyLight(x, y, 0, propagated);
                            lightQueue.push({x, y, 0, propagated});
                        }
                    }
                }

                if (southChunk && !Block::byId(chunk->getBlockId(x, y, Chunk::SIZE_Z - 1))->isSolid()) {
                    uint8_t neighborLight = southChunk->getSkyLight(x, y, 0);
                    if (neighborLight > 1) {
                        uint8_t propagated = neighborLight - 1;
                        if (propagated > chunk->getSkyLight(x, y, Chunk::SIZE_Z - 1)) {
                            chunk->setSkyLight(x, y, Chunk::SIZE_Z - 1, propagated);
                            lightQueue.push({x, y, Chunk::SIZE_Z - 1, propagated});
                        }
                    }
                }
            }
        }

        while (!lightQueue.empty()) {
            LightNode node = lightQueue.front();
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
    }

    m_pool->detach_task([this, pos] {
        if (!m_running.load()) {
            std::lock_guard<std::mutex> genLock(m_generatingMutex);
            m_generatingChunks.erase(pos);
            return;
        }

        generateChunk(pos);

        std::lock_guard<std::mutex> genLock(m_generatingMutex);
        m_generatingChunks.erase(pos);
    });
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
