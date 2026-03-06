#include "ChunkCache.h"

ChunkCache::ChunkCache() : m_entries() {}

void ChunkCache::put(const ChunkPos &pos, Chunk *chunk)
{
    if (!chunk)
    {
        return;
    }

    m_entries[pos] = chunk;
}

void ChunkCache::erase(const ChunkPos &pos) { m_entries.erase(pos); }

void ChunkCache::clear() { m_entries.clear(); }

Chunk *ChunkCache::get(const ChunkPos &pos)
{
    auto it = m_entries.find(pos);
    if (it == m_entries.end())
    {
        return nullptr;
    }
    return it->second;
}

const Chunk *ChunkCache::get(const ChunkPos &pos) const
{
    auto it = m_entries.find(pos);
    if (it == m_entries.end())
    {
        return nullptr;
    }
    return it->second;
}

size_t ChunkCache::size() const { return m_entries.size(); }
