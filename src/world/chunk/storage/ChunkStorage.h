#pragma once

#include "../Chunk.h"

class ChunkStorage
{
public:
    virtual ~ChunkStorage() = default;

    virtual bool loadChunk(Chunk *chunk) = 0;
    virtual void saveChunk(const Chunk &chunk) = 0;

    virtual void flush() = 0;
};
