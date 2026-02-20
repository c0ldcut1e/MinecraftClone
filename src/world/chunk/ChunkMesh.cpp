#include "ChunkMesh.h"

#include <atomic>
#include <mutex>
#include <utility>

#include <glad/glad.h>

#include "../../rendering/RenderCommand.h"

static std::atomic<uint64_t> s_meshId{1};

ChunkMesh::ChunkMesh(Texture *texture) : m_texture(texture), m_vao(0), m_vbo(0), m_vertexCount(0), m_id(s_meshId.fetch_add(1, std::memory_order_relaxed)) {
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 8 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 8 * sizeof(float), 3 * sizeof(float));

    RenderCommand::enableVertexAttrib(2);
    RenderCommand::setVertexAttribPointer(2, 3, GL_FLOAT, false, 8 * sizeof(float), 5 * sizeof(float));
}

ChunkMesh::~ChunkMesh() {
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void ChunkMesh::upload(const float *vertices, uint32_t vertexCount, std::vector<uint16_t> &&rawLights, std::vector<float> &&shades, std::vector<uint32_t> &&tints) {
    m_vertexCount = vertexCount / 8;

    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);

        m_vertices.assign(vertices, vertices + vertexCount);
        m_rawLights = std::move(rawLights);
        m_shades    = std::move(shades);
        m_tints     = std::move(tints);
    }

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(m_vertices.data(), (uint32_t) (m_vertices.size() * sizeof(float)), GL_DYNAMIC_DRAW);
}

uint64_t ChunkMesh::getId() const { return m_id; }

void ChunkMesh::snapshotSky(std::vector<float> &vertices, std::vector<uint16_t> &rawLights, std::vector<float> &shades, std::vector<uint32_t> &tints) const {
    std::lock_guard<std::mutex> lock(m_cpuMutex);
    vertices  = m_vertices;
    rawLights = m_rawLights;
    shades    = m_shades;
    tints     = m_tints;
}

void ChunkMesh::applySky(std::vector<float> &&vertices) {
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);

        m_vertices = std::move(vertices);
    }

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::bufferSubData(GL_ARRAY_BUFFER, 0, (uint32_t) (m_vertices.size() * sizeof(float)), m_vertices.data());
}

void ChunkMesh::render() const {
    if (!m_texture) return;

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_texture->getId());

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, (int) m_vertexCount);
}
