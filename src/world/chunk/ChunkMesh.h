#pragma once

#include <cstdint>

#include "../../graphics/Texture.h"

class ChunkMesh {
public:
    ChunkMesh(Texture *texture);
    ~ChunkMesh();

    void upload(const float *vertices, uint32_t vertexCount);
    void draw() const;

private:
    Texture *m_texture;
    uint32_t m_vao;
    uint32_t m_vbo;
    uint32_t m_vertexCount;
};
