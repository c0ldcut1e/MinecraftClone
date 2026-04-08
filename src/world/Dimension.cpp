#include "Dimension.h"

#include <cmath>

#include "../core/Logger.h"
#include "../utils/math/Mth.h"
#include "block/Block.h"

Dimension::Dimension() : m_emptyChunksSolid(true), m_renderDistance(16) {}

Chunk *Dimension::getChunk(const ChunkPos &pos)
{
    if (Chunk *cached = m_chunkCache.get(pos))
    {
        return cached;
    }

    auto it = m_chunks.find(pos);
    if (it == m_chunks.end())
    {
        return nullptr;
    }

    m_chunkCache.put(pos, it->second.get());
    return it->second.get();
}

const Chunk *Dimension::getChunk(const ChunkPos &pos) const
{
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end())
    {
        return nullptr;
    }

    return it->second.get();
}

Chunk &Dimension::createChunk(const ChunkPos &pos)
{
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end())
    {
        return *it->second;
    }

    std::unique_ptr<Chunk> chunk = std::make_unique<Chunk>(pos);
    Chunk &ref                   = *chunk;

    m_chunks.emplace(pos, std::move(chunk));
    m_chunkCache.put(pos, &ref);

    Logger::logInfo("Created chunk (%d, %d, %d)", pos.x, pos.y, pos.z);

    return ref;
}

bool Dimension::adoptChunk(const ChunkPos &pos, std::unique_ptr<Chunk> chunk)
{
    if (!chunk)
    {
        return false;
    }

    auto [it, inserted] = m_chunks.emplace(pos, std::move(chunk));
    if (!inserted)
    {
        return false;
    }

    m_chunkCache.put(pos, it->second.get());
    return true;
}

bool Dimension::hasChunk(const ChunkPos &pos) const { return m_chunks.find(pos) != m_chunks.end(); }

const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &
Dimension::getChunks() const
{
    return m_chunks;
}

void Dimension::markChunkDirty(const BlockPos &pos)
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);

    int cx = Mth::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Mth::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Mth::floorDiv(pos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos{cx, cy, cz};
    if (m_dirtyChunksSet.insert(chunkPos).second)
    {
        m_dirtyChunks.push_back(chunkPos);
    }
}

void Dimension::markChunkDirtyUrgent(const ChunkPos &pos)
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    if (m_urgentDirtyChunksSet.insert(pos).second)
    {
        m_urgentDirtyChunks.push_front(pos);
    }
}

bool Dimension::pollDirtyChunk(ChunkPos *outPos)
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    if (m_dirtyChunks.empty())
    {
        return false;
    }

    *outPos = m_dirtyChunks.front();
    m_dirtyChunks.pop_front();
    m_dirtyChunksSet.erase(*outPos);
    return true;
}

bool Dimension::pollUrgentDirtyChunk(ChunkPos *outPos)
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    if (m_urgentDirtyChunks.empty())
    {
        return false;
    }

    *outPos = m_urgentDirtyChunks.front();
    m_urgentDirtyChunks.pop_front();
    m_urgentDirtyChunksSet.erase(*outPos);
    return true;
}

void Dimension::clearDirtyChunks()
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);

    m_dirtyChunks.clear();
    m_dirtyChunksSet.clear();
    m_urgentDirtyChunks.clear();
    m_urgentDirtyChunksSet.clear();
}

const std::deque<ChunkPos> &Dimension::getDirtyChunks() const { return m_dirtyChunks; }

size_t Dimension::getQueuedDirtyChunkCount() const
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_dirtyChunks.size();
}

size_t Dimension::getUrgentDirtyChunkCount() const
{
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_urgentDirtyChunks.size();
}

uint32_t Dimension::getBlockId(const BlockPos &pos) const
{
    int cx = Mth::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Mth::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Mth::floorDiv(pos.z, Chunk::SIZE_Z);

    int lx = Mth::floorMod(pos.x, Chunk::SIZE_X);
    int ly = Mth::floorMod(pos.y, Chunk::SIZE_Y);
    int lz = Mth::floorMod(pos.z, Chunk::SIZE_Z);

    const Chunk *chunk = getChunk(ChunkPos{cx, cy, cz});
    if (!chunk)
    {
        return 0;
    }
    if (ly < 0 || ly >= Chunk::SIZE_Y)
    {
        return 0;
    }

    return chunk->getBlockId(lx, ly, lz);
}

int Dimension::getSurfaceHeight(int levelX, int levelZ) const
{
    int chunkX = levelX / Chunk::SIZE_X;
    int chunkZ = levelZ / Chunk::SIZE_Z;

    int localX = levelX % Chunk::SIZE_X;
    int localZ = levelZ % Chunk::SIZE_Z;

    if (localX < 0)
    {
        localX += Chunk::SIZE_X;
    }
    if (localZ < 0)
    {
        localZ += Chunk::SIZE_Z;
    }

    ChunkPos pos{chunkX, 0, chunkZ};

    auto it = m_chunks.find(pos);
    if (it == m_chunks.end())
    {
        return 0;
    }

    const Chunk &chunk = *it->second;

    for (int y = Chunk::SIZE_Y - 1; y >= 0; y--)
    {
        if (Block::byId(chunk.getBlockId(localX, y, localZ))->isSolid())
        {
            return y;
        }
    }

    return 0;
}

bool Dimension::intersectsBlock(const AABB &aabb) const
{
    int minX = (int) floor(aabb.getMin().x);
    int minY = (int) floor(aabb.getMin().y);
    int minZ = (int) floor(aabb.getMin().z);

    int maxX = (int) floor(aabb.getMax().x - 1e-6);
    int maxY = (int) floor(aabb.getMax().y - 1e-6);
    int maxZ = (int) floor(aabb.getMax().z - 1e-6);

    for (int z = minZ; z <= maxZ; z++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                int cx = Mth::floorDiv(x, Chunk::SIZE_X);
                int cy = Mth::floorDiv(y, Chunk::SIZE_Y);
                int cz = Mth::floorDiv(z, Chunk::SIZE_Z);

                int lx = Mth::floorMod(x, Chunk::SIZE_X);
                int lz = Mth::floorMod(z, Chunk::SIZE_Z);

                const Chunk *chunk = getChunk({cx, cy, cz});
                if (!chunk)
                {
                    if (m_emptyChunksSolid)
                    {
                        return true;
                    }
                    continue;
                }

                Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
                if (!block->isSolid())
                {
                    continue;
                }

                AABB blockBox(Vec3(x, y, z), Vec3(x + 1, y + 1, z + 1));
                if (aabb.intersects(blockBox))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void Dimension::setEmptyChunksSolid(bool solid) { m_emptyChunksSolid = solid; }

bool Dimension::areEmptyChunksSolid() const { return m_emptyChunksSolid; }

void Dimension::queueLightUpdate(const BlockPos &pos) { m_lightUpdates.push_back(pos); }

bool Dimension::pollLightUpdate(BlockPos *outPos)
{
    if (m_lightUpdates.empty())
    {
        return false;
    }

    *outPos = m_lightUpdates.front();
    m_lightUpdates.pop_front();
    return true;
}

size_t Dimension::getQueuedLightUpdateCount() const { return m_lightUpdates.size(); }

void Dimension::setRenderDistance(int distance) { m_renderDistance = distance; }

int Dimension::getRenderDistance() const { return m_renderDistance; }

Fog &Dimension::getFog() { return m_fog; }

const Fog &Dimension::getFog() const { return m_fog; }

void Dimension::advanceTime() { m_dimensionTime.advance(); }

const DimensionTime &Dimension::getDimensionTime() const { return m_dimensionTime; }

void Dimension::setDimensionTime(uint64_t ticks) { m_dimensionTime.setTicks(ticks); }

float Dimension::getDayFraction() const { return m_dimensionTime.getDayFraction(); }

float Dimension::getCelestialAngle() const { return m_dimensionTime.getCelestialAngle(); }

float Dimension::getDaylightFactor() const { return m_dimensionTime.getDaylightFactor(); }

uint8_t Dimension::getSkyLightClamp() const { return m_dimensionTime.getSkyLightClamp(); }

void Dimension::setDaylightCurveTicks(int darkStartTick, int darkPeakTick)
{
    m_dimensionTime.setDaylightCurve(darkStartTick, darkPeakTick);
}

int Dimension::getDarkStartTick() const { return m_dimensionTime.getDarkStartTick(); }

int Dimension::getDarkPeakTick() const { return m_dimensionTime.getDarkPeakTick(); }
