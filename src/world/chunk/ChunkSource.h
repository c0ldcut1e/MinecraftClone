#pragma once

#include "Chunk.h"

class ChunkSource
{
public:
    virtual ~ChunkSource() = default;

    virtual const Chunk *getChunk(const ChunkPos &pos) const = 0;
};
