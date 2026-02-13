#include "ChunkMesh.h"
#include "../../graphics/RenderCommand.h"

ChunkMesh::ChunkMesh(Texture *texture) : m_texture(texture), m_vao(0), m_vbo(0), m_vertexCount(0) {
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, RC_FLOAT, false, 8 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, RC_FLOAT, false, 8 * sizeof(float), 3 * sizeof(float));

    RenderCommand::enableVertexAttrib(2);
    RenderCommand::setVertexAttribPointer(2, 3, RC_FLOAT, false, 8 * sizeof(float), 5 * sizeof(float));
}

ChunkMesh::~ChunkMesh() {
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void ChunkMesh::upload(const float *vertices, uint32_t vertexCount) {
    m_vertexCount = vertexCount;

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(vertices, vertexCount * sizeof(float), RC_STATIC_DRAW);
}

void ChunkMesh::draw() const {
    if (!m_vertexCount) return;

    m_texture->bind(0);
    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::drawArrays(RC_TRIANGLES, 0, m_vertexCount / 6);
}
