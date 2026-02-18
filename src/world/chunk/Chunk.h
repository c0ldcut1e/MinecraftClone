#pragma once

#include <cstdint>

#include "../block/Block.h"
#include "ChunkPos.h"

class Chunk {
public:
    static constexpr int SIZE_X = 16;
    static constexpr int SIZE_Y = 256;
    static constexpr int SIZE_Z = 16;

    explicit Chunk(const ChunkPos &pos);

    uint32_t getBlockId(int x, int y, int z) const;
    void setBlock(int x, int y, int z, Block *block);

    const ChunkPos &getPos() const;

    void getLight(int x, int y, int z, uint8_t &r, uint8_t &g, uint8_t &b) const;
    void setLight(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b);

    void getBlockLight(int x, int y, int z, uint8_t &r, uint8_t &g, uint8_t &b) const;
    void setBlockLight(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b);

    uint8_t getSkyLight(int x, int y, int z) const;
    void setSkyLight(int x, int y, int z, uint8_t level);

private:
    int index(int x, int y, int z) const;

    ChunkPos m_pos;
    uint32_t m_blocks[SIZE_X * SIZE_Y * SIZE_Z];

    struct LightData {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    LightData m_blockLight[SIZE_X * SIZE_Y * SIZE_Z];
    uint8_t m_skyLight[SIZE_X * SIZE_Y * SIZE_Z];
    bool m_needsRelight;
};
