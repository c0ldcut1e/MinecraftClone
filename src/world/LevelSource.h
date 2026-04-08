#pragma once

#include "block/BlockSource.h"
#include "chunk/ChunkSource.h"

class Level;

class LevelSource : public BlockSource, public ChunkSource
{
public:
    explicit LevelSource(const Level *level);

    uint32_t getBlockId(const BlockPos &pos) const override;
    const Chunk *getChunk(const ChunkPos &pos) const override;

private:
    const Level *m_level;
};
