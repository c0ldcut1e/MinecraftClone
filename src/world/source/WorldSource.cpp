#include "WorldSource.h"

#include "../World.h"

WorldSource::WorldSource(const World *world) : m_world(world) {}

uint32_t WorldSource::getBlockId(const BlockPos &pos) const
{
    if (!m_world)
    {
        return 0;
    }

    return m_world->getBlockId(pos);
}

const Chunk *WorldSource::getChunk(const ChunkPos &pos) const
{
    if (!m_world)
    {
        return nullptr;
    }

    return m_world->getChunk(pos);
}
