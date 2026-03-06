#pragma once

#include <cstddef>
#include <unordered_map>

#include "../Chunk.h"
#include "../ChunkPos.h"

class ChunkCache
{
public:
    ChunkCache();

    void put(const ChunkPos &pos, Chunk *chunk);
    void erase(const ChunkPos &pos);
    void clear();

    Chunk *get(const ChunkPos &pos);
    const Chunk *get(const ChunkPos &pos) const;

    size_t size() const;

private:
    std::unordered_map<ChunkPos, Chunk *, ChunkPosHash> m_entries;
};
