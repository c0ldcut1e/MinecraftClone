#include "ChunkMesh.h"

#include <atomic>
#include <mutex>
#include <utility>

#include <glad/glad.h>

#include "../../rendering/RenderCommand.h"

static std::atomic<uint64_t> s_meshId{1};

ChunkMesh::ChunkMesh(Texture *texture)
    : m_texture(texture), m_vao(0), m_vbo(0), m_tintModeVbo(0), m_rawLightVbo(0), m_shadeVbo(0),
      m_vertexCount(0),
      m_id(s_meshId.fetch_add(1, std::memory_order_relaxed))
{
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();
    m_tintModeVbo = RenderCommand::createBuffer();
    m_rawLightVbo = RenderCommand::createBuffer();
    m_shadeVbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 8 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 8 * sizeof(float),
                                          3 * sizeof(float));

    RenderCommand::enableVertexAttrib(2);
    RenderCommand::setVertexAttribPointer(2, 3, GL_FLOAT, false, 8 * sizeof(float),
                                          5 * sizeof(float));

    RenderCommand::bindArrayBuffer(m_tintModeVbo);
    RenderCommand::enableVertexAttrib(3);
    RenderCommand::setVertexAttribPointer(3, 4, GL_FLOAT, false, 4 * sizeof(float), 0);

    RenderCommand::bindArrayBuffer(m_rawLightVbo);
    RenderCommand::enableVertexAttrib(4);
    RenderCommand::setVertexAttribPointer(4, 4, GL_FLOAT, false, 4 * sizeof(float), 0);

    RenderCommand::bindArrayBuffer(m_shadeVbo);
    RenderCommand::enableVertexAttrib(5);
    RenderCommand::setVertexAttribPointer(5, 1, GL_FLOAT, false, sizeof(float), 0);
}

ChunkMesh::~ChunkMesh()
{
    RenderCommand::deleteBuffer(m_shadeVbo);
    RenderCommand::deleteBuffer(m_rawLightVbo);
    RenderCommand::deleteBuffer(m_tintModeVbo);
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void ChunkMesh::upload(const float *vertices, uint32_t vertexCount,
                       std::vector<uint16_t> &&rawLights, std::vector<float> &&shades,
                       std::vector<uint32_t> &&tints)
{
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
    RenderCommand::uploadArrayBuffer(
            m_vertices.data(), (uint32_t) (m_vertices.size() * sizeof(float)), GL_DYNAMIC_DRAW);

    std::vector<float> tintModeData;
    tintModeData.resize(m_tints.size() * 4);
    for (size_t i = 0; i < m_tints.size(); i++)
    {
        uint32_t packedTint = m_tints[i];
        uint8_t mode        = (uint8_t) ((packedTint >> 24) & 0xFF);
        uint8_t tr          = (uint8_t) ((packedTint >> 16) & 0xFF);
        uint8_t tg          = (uint8_t) ((packedTint >> 8) & 0xFF);
        uint8_t tb          = (uint8_t) (packedTint & 0xFF);

        size_t off            = i * 4;
        tintModeData[off + 0] = (float) tr / 255.0f;
        tintModeData[off + 1] = (float) tg / 255.0f;
        tintModeData[off + 2] = (float) tb / 255.0f;
        tintModeData[off + 3] = (float) mode / 255.0f;
    }

    RenderCommand::bindArrayBuffer(m_tintModeVbo);
    RenderCommand::uploadArrayBuffer(tintModeData.data(),
                                     (uint32_t) (tintModeData.size() * sizeof(float)),
                                     GL_DYNAMIC_DRAW);

    std::vector<float> rawLightData;
    rawLightData.resize(m_rawLights.size() * 4);
    for (size_t i = 0; i < m_rawLights.size(); i++)
    {
        uint16_t raw = m_rawLights[i];

        size_t off           = i * 4;
        rawLightData[off + 0] = (float) (raw & 15);
        rawLightData[off + 1] = (float) ((raw >> 4) & 15);
        rawLightData[off + 2] = (float) ((raw >> 8) & 15);
        rawLightData[off + 3] = (float) ((raw >> 12) & 15);
    }

    RenderCommand::bindArrayBuffer(m_rawLightVbo);
    RenderCommand::uploadArrayBuffer(rawLightData.data(),
                                     (uint32_t) (rawLightData.size() * sizeof(float)),
                                     GL_DYNAMIC_DRAW);

    RenderCommand::bindArrayBuffer(m_shadeVbo);
    RenderCommand::uploadArrayBuffer(m_shades.data(),
                                     (uint32_t) (m_shades.size() * sizeof(float)),
                                     GL_DYNAMIC_DRAW);
}

uint64_t ChunkMesh::getId() const { return m_id; }

void ChunkMesh::snapshotSky(std::vector<float> *vertices, std::vector<uint16_t> *rawLights,
                            std::vector<float> *shades, std::vector<uint32_t> *tints) const
{
    std::lock_guard<std::mutex> lock(m_cpuMutex);
    *vertices  = m_vertices;
    *rawLights = m_rawLights;
    *shades    = m_shades;
    *tints     = m_tints;
}

void ChunkMesh::applySky(std::vector<float> &&vertices)
{
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);

        m_vertices = std::move(vertices);
    }

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::bufferSubData(GL_ARRAY_BUFFER, 0, (uint32_t) (m_vertices.size() * sizeof(float)),
                                 m_vertices.data());
}

void ChunkMesh::render() const
{
    if (!m_texture)
    {
        return;
    }

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_texture->getId());

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, (int) m_vertexCount);
}
