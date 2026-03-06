#include "OldChunkStorage.h"

#include <utility>

#include "../../block/Block.h"

OldChunkStorage::OldChunkStorage() : m_entries() {}

bool OldChunkStorage::loadChunk(Chunk *chunk)
{
    if (!chunk)
    {
        return false;
    }

    const ChunkPos pos = chunk->getPos();

    for (const Entry &entry : m_entries)
    {
        if (entry.pos != pos)
        {
            continue;
        }

        size_t index = 0;
        for (int y = 0; y < Chunk::SIZE_Y; y++)
        {
            for (int z = 0; z < Chunk::SIZE_Z; z++)
            {
                for (int x = 0; x < Chunk::SIZE_X; x++)
                {
                    if (index < entry.blockIds.size())
                    {
                        chunk->setBlock(x, y, z, Block::byId(entry.blockIds[index++]));
                    }
                }
            }
        }

        return true;
    }

    return false;
}

void OldChunkStorage::saveChunk(const Chunk &chunk)
{
    Entry entry;
    entry.pos = chunk.getPos();
    entry.blockIds.reserve((size_t) Chunk::SIZE_X * Chunk::SIZE_Y * Chunk::SIZE_Z);

    for (int y = 0; y < Chunk::SIZE_Y; y++)
    {
        for (int z = 0; z < Chunk::SIZE_Z; z++)
        {
            for (int x = 0; x < Chunk::SIZE_X; x++)
            {
                entry.blockIds.push_back(chunk.getBlockId(x, y, z));
            }
        }
    }

    for (Entry &existing : m_entries)
    {
        if (existing.pos == entry.pos)
        {
            existing.blockIds = std::move(entry.blockIds);
            return;
        }
    }

    m_entries.push_back(std::move(entry));
}

void OldChunkStorage::flush() {}
