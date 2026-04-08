#include "LevelSource.h"

#include "Level.h"

LevelSource::LevelSource(const Level *level) : m_level(level) {}

uint32_t LevelSource::getBlockId(const BlockPos &pos) const
{
    if (!m_level)
    {
        return 0;
    }

    return m_level->getBlockId(pos);
}

const Chunk *LevelSource::getChunk(const ChunkPos &pos) const
{
    if (!m_level)
    {
        return nullptr;
    }

    return m_level->getChunk(pos);
}
