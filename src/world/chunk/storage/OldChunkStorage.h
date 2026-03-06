#pragma once

#include <vector>

#include "ChunkStorage.h"

class OldChunkStorage : public ChunkStorage
{
public:
    OldChunkStorage();

    bool loadChunk(Chunk *chunk) override;
    void saveChunk(const Chunk &chunk) override;

    void flush() override;

private:
    struct Entry
    {
        ChunkPos pos;
        std::vector<uint32_t> blockIds;
    };

    std::vector<Entry> m_entries;
};
