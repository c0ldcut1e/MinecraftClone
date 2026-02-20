#include "Chunk.h"

#include "../block/BlockRegistry.h"

Chunk::Chunk(const ChunkPos &pos) : m_pos(pos), m_needsRelight(true) {
    for (int i = 0; i < SIZE_X * SIZE_Y * SIZE_Z; i++) {
        m_blocks[i]       = 0;
        m_blockLight[i].r = 0;
        m_blockLight[i].g = 0;
        m_blockLight[i].b = 0;
        m_skyLight[i]     = 0;
    }

    for (int i = 0; i < SIZE_X * SIZE_Z; i++) m_columnBiomes[i] = nullptr;
}

uint32_t Chunk::getBlockId(int x, int y, int z) const { return m_blocks[index(x, y, z)]; }

void Chunk::setBlock(int x, int y, int z, Block *block) {
    uint32_t id              = BlockRegistry::get()->idOf(block);
    m_blocks[index(x, y, z)] = id;
}

const ChunkPos &Chunk::getPos() const { return m_pos; }

int Chunk::columnIndex(int x, int z) const { return x + SIZE_X * z; }

void Chunk::setBiomeAt(int x, int z, Biome *biome) { m_columnBiomes[columnIndex(x, z)] = biome; }

Biome *Chunk::getBiomeAt(int x, int z) const { return m_columnBiomes[columnIndex(x, z)]; }

void Chunk::getLight(int x, int y, int z, uint8_t &r, uint8_t &g, uint8_t &b) const {
    uint8_t br, bg, bb;
    getBlockLight(x, y, z, br, bg, bb);

    uint8_t sky = getSkyLight(x, y, z);

    r = br > sky ? br : sky;
    g = bg > sky ? bg : sky;
    b = bb > sky ? bb : sky;
}

void Chunk::setLight(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b) {
    LightData &data = m_blockLight[index(x, y, z)];
    data.r          = r;
    data.g          = g;
    data.b          = b;
}

void Chunk::getBlockLight(int x, int y, int z, uint8_t &r, uint8_t &g, uint8_t &b) const {
    LightData data = m_blockLight[index(x, y, z)];
    r              = data.r;
    g              = data.g;
    b              = data.b;
}

void Chunk::setBlockLight(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b) {
    LightData &data = m_blockLight[index(x, y, z)];
    data.r          = r;
    data.g          = g;
    data.b          = b;
}

uint8_t Chunk::getSkyLight(int x, int y, int z) const { return m_skyLight[index(x, y, z)]; }

void Chunk::setSkyLight(int x, int y, int z, uint8_t level) { m_skyLight[index(x, y, z)] = level; }

int Chunk::index(int x, int y, int z) const { return x + SIZE_X * (y + SIZE_Y * z); }
