#pragma once

#include "BlockSource.h"
#include "ChunkSource.h"

class World;

class WorldSource : public BlockSource, public ChunkSource
{
public:
    explicit WorldSource(const World *world);

    uint32_t getBlockId(const BlockPos &pos) const override;
    const Chunk *getChunk(const ChunkPos &pos) const override;

private:
    const World *m_world;
};
