#include "ChunkManager.h"

#include <cmath>

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

    int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 2;
    threadCount /= 2;
    Logger::logInfo("Starting chunk generation with %d threads", threadCount);

    for (int i = 0; i < threadCount; i++) m_workers.emplace_back(&ChunkManager::workerThread, this);
}

void ChunkManager::stop() {
    if (!m_running.load()) return;

    m_running.store(false);
    m_queueCondition.notify_all();

    for (std::thread &worker : m_workers)
        if (worker.joinable()) worker.join();

    m_workers.clear();
    Logger::logInfo("Chunk generation stopped");
}

void ChunkManager::update(const Vec3 &playerPosition) {
    int cx = Math::floorDiv((int) playerPosition.x, Chunk::SIZE_X);
    int cy = Math::floorDiv((int) playerPosition.y, Chunk::SIZE_Y);
    int cz = Math::floorDiv((int) playerPosition.z, Chunk::SIZE_Z);
    ChunkPos playerChunk{cx, cy, cz};
    if (playerChunk.x == m_lastPlayerChunk.x && playerChunk.y == m_lastPlayerChunk.y && playerChunk.z == m_lastPlayerChunk.z) return;
    m_lastPlayerChunk = playerChunk;

    std::vector<GenerationTask> newTasks;

    int renderDistance = m_world->getRenderDistance();
    for (int dz = -renderDistance; dz <= renderDistance; dz++)
        for (int dx = -renderDistance; dx <= renderDistance; dx++) {
            ChunkPos pos{cx + dx, 0, cz + dz};
            if (!isChunkInRenderDistance(pos, playerChunk)) continue;
            if (!m_world->hasChunk(pos)) {
                int priority = calculatePriority(pos, playerChunk);
                newTasks.push_back({pos, priority});
            }
        }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        for (ChunkManager::GenerationTask &task : newTasks) m_taskQueue.push(task);
    }

    m_queueCondition.notify_all();
}

void ChunkManager::workerThread() {
    while (m_running.load()) {
        GenerationTask task;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] { return !m_running.load() || !m_taskQueue.empty(); });

            if (!m_running.load()) break;
            if (m_taskQueue.empty()) continue;

            task = m_taskQueue.top();
            m_taskQueue.pop();
        }

        generateChunk(task.pos);

        {
            std::lock_guard<std::mutex> lock(m_generatingMutex);

            m_generatingChunks.erase(task.pos);
        }
    }
}

void ChunkManager::generateChunk(const ChunkPos &pos) {
    if (m_world->hasChunk(pos)) return;

    TerrainGenerator generator(0xDEADCAFE);

    Chunk &chunk = m_world->createChunk(pos);

    for (int z = 0; z < Chunk::SIZE_Z; z++)
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            int worldX = pos.x * Chunk::SIZE_X + x;
            int worldZ = pos.z * Chunk::SIZE_Z + z;

            int height = generator.getHeightAt(worldX, worldZ);

            chunk.setBlock(x, 0, z, Block::byName("bedrock"));

            for (int y = 1; y <= height && y < Chunk::SIZE_Y; y++) {
                if (y < height - 4) {
                    float caveValue = generator.getCaveValue(worldX, y, worldZ);
                    float threshold = 0.35f;
                    if (caveValue > threshold) continue;
                }

                if (y == height) chunk.setBlock(x, y, z, Block::byName("grass"));
                else if (y >= height - 4)
                    chunk.setBlock(x, y, z, Block::byName("dirt"));
                else
                    chunk.setBlock(x, y, z, Block::byName("stone"));
            }
        }

    for (int z = 0; z < Chunk::SIZE_Z; z++)
        for (int x = 0; x < Chunk::SIZE_X; x++) {
            bool blocked = false;

            for (int y = Chunk::SIZE_Y - 1; y >= 0; y--) {
                if (blocked) {
                    chunk.setSkyLight(x, y, z, 0);
                    continue;
                }

                Block *block = Block::byId(chunk.getBlockId(x, y, z));

                if (block->isSolid()) {
                    blocked = true;
                    chunk.setSkyLight(x, y, z, 0);
                } else
                    chunk.setSkyLight(x, y, z, 15);
            }
        }

    int worldX = pos.x * Chunk::SIZE_X;
    int worldY = pos.y * Chunk::SIZE_Y;
    int worldZ = pos.z * Chunk::SIZE_Z;
    m_world->markChunkDirty(worldX, worldY, worldZ);
}

void ChunkManager::queueChunkGeneration(const ChunkPos &pos, int priority) {
    {
        std::lock_guard<std::mutex> lock(m_generatingMutex);

        if (m_generatingChunks.find(pos) != m_generatingChunks.end()) return;
        m_generatingChunks.insert(pos);
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        m_taskQueue.push({pos, priority});
    }

    m_queueCondition.notify_one();
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
