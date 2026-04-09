#include "ChunkMesh.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <utility>

#include <glad/glad.h>

#include "../../rendering/RenderCommand.h"
#include "../../utils/math/Mth.h"
#include "../../utils/math/Vec3.h"
#include "Chunk.h"

static std::atomic<uint64_t> s_meshId{1};

static uint16_t packPositionComponent(float value, float maxValue)
{
    if (maxValue <= 0.0f)
    {
        return 0;
    }

    float normalized = Mth::clampf(value / maxValue, 0.0f, 1.0f);
    return (uint16_t) (normalized * 65535.0f + 0.5f);
}

static uint8_t packNormalizedByte(float value)
{
    float normalized = Mth::clampf(value, 0.0f, 1.0f);
    return (uint8_t) (normalized * 255.0f + 0.5f);
}

static int8_t packSignedNormalizedByte(double value)
{
    double normalized = Mth::clamp(value, -1.0, 1.0);
    return (int8_t) std::lround(normalized * 127.0);
}

static uint8_t packLightNibble(uint8_t value)
{
    if (value > 15)
    {
        value = 15;
    }
    return (uint8_t) (value * 17);
}

ChunkMesh::ChunkMesh(Texture *texture)
    : m_texture(texture), m_vao(0), m_positionVbo(0), m_uvVbo(0), m_lightVbo(0), m_atlasRectVbo(0),
      m_tintModeVbo(0), m_rawLightVbo(0), m_shadeVbo(0), m_normalVbo(0), m_vertexCount(0),
      m_id(s_meshId.fetch_add(1, std::memory_order_relaxed))
{
    m_vao          = RenderCommand::createVertexArray();
    m_positionVbo  = RenderCommand::createBuffer();
    m_uvVbo        = RenderCommand::createBuffer();
    m_lightVbo     = RenderCommand::createBuffer();
    m_atlasRectVbo = RenderCommand::createBuffer();
    m_tintModeVbo  = RenderCommand::createBuffer();
    m_rawLightVbo  = RenderCommand::createBuffer();
    m_shadeVbo     = RenderCommand::createBuffer();
    m_normalVbo    = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_positionVbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_UNSIGNED_SHORT, true, 3 * sizeof(uint16_t), 0);

    RenderCommand::bindArrayBuffer(m_uvVbo);
    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 2 * sizeof(float), 0);

    RenderCommand::bindArrayBuffer(m_lightVbo);
    RenderCommand::enableVertexAttrib(2);
    RenderCommand::setVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, true, 3 * sizeof(uint8_t), 0);

    RenderCommand::bindArrayBuffer(m_atlasRectVbo);
    RenderCommand::enableVertexAttrib(6);
    RenderCommand::setVertexAttribPointer(6, 4, GL_FLOAT, false, 4 * sizeof(float), 0);

    RenderCommand::bindArrayBuffer(m_tintModeVbo);
    RenderCommand::enableVertexAttrib(3);
    RenderCommand::setVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, true, 4 * sizeof(uint8_t), 0);

    RenderCommand::bindArrayBuffer(m_rawLightVbo);
    RenderCommand::enableVertexAttrib(4);
    RenderCommand::setVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, true, 4 * sizeof(uint8_t), 0);

    RenderCommand::bindArrayBuffer(m_shadeVbo);
    RenderCommand::enableVertexAttrib(5);
    RenderCommand::setVertexAttribPointer(5, 1, GL_UNSIGNED_BYTE, true, sizeof(uint8_t), 0);

    RenderCommand::bindArrayBuffer(m_normalVbo);
    RenderCommand::enableVertexAttrib(7);
    RenderCommand::setVertexAttribPointer(7, 3, GL_BYTE, true, 3 * sizeof(int8_t), 0);
}

ChunkMesh::~ChunkMesh()
{
    RenderCommand::deleteBuffer(m_normalVbo);
    RenderCommand::deleteBuffer(m_shadeVbo);
    RenderCommand::deleteBuffer(m_rawLightVbo);
    RenderCommand::deleteBuffer(m_tintModeVbo);
    RenderCommand::deleteBuffer(m_atlasRectVbo);
    RenderCommand::deleteBuffer(m_lightVbo);
    RenderCommand::deleteBuffer(m_uvVbo);
    RenderCommand::deleteBuffer(m_positionVbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void ChunkMesh::upload(const float *vertices, uint32_t vertexCount, std::vector<float> &&atlasRects,
                       std::vector<uint16_t> &&rawLights, std::vector<float> &&shades,
                       std::vector<uint32_t> &&tints)
{
    m_vertexCount = vertexCount / 8;

    std::vector<uint16_t> positionData;
    std::vector<float> uvData;
    std::vector<uint8_t> lightData;
    std::vector<int8_t> normalData;
    positionData.resize((size_t) m_vertexCount * 3);
    uvData.resize((size_t) m_vertexCount * 2);
    lightData.resize((size_t) m_vertexCount * 3);
    normalData.resize((size_t) m_vertexCount * 3);

    for (uint32_t i = 0; i < m_vertexCount; i++)
    {
        size_t src = (size_t) i * 8;
        size_t pos = (size_t) i * 3;
        size_t uv  = (size_t) i * 2;

        positionData[pos + 0] = packPositionComponent(vertices[src + 0], (float) Chunk::SIZE_X);
        positionData[pos + 1] = packPositionComponent(vertices[src + 1], (float) Chunk::SIZE_Y);
        positionData[pos + 2] = packPositionComponent(vertices[src + 2], (float) Chunk::SIZE_Z);

        uvData[uv + 0] = vertices[src + 3];
        uvData[uv + 1] = vertices[src + 4];

        lightData[pos + 0] = packNormalizedByte(vertices[src + 5]);
        lightData[pos + 1] = packNormalizedByte(vertices[src + 6]);
        lightData[pos + 2] = packNormalizedByte(vertices[src + 7]);
    }

    for (uint32_t i = 0; i < m_vertexCount; i += 6)
    {
        size_t src0 = (size_t) i * 8;
        size_t src1 = (size_t) std::min(i + 1, m_vertexCount - 1) * 8;
        size_t src2 = (size_t) std::min(i + 2, m_vertexCount - 1) * 8;

        Vec3 p0(vertices[src0 + 0], vertices[src0 + 1], vertices[src0 + 2]);
        Vec3 p1(vertices[src1 + 0], vertices[src1 + 1], vertices[src1 + 2]);
        Vec3 p2(vertices[src2 + 0], vertices[src2 + 1], vertices[src2 + 2]);
        Vec3 normal = p1.sub(p0).cross(p2.sub(p0)).normalize();

        if (normal.lengthSquared() == 0.0)
        {
            normal = Vec3(0.0, 1.0, 0.0);
        }

        int8_t nx = packSignedNormalizedByte(normal.x);
        int8_t ny = packSignedNormalizedByte(normal.y);
        int8_t nz = packSignedNormalizedByte(normal.z);

        for (uint32_t j = 0; j < 6 && i + j < m_vertexCount; j++)
        {
            size_t dst          = (size_t) (i + j) * 3;
            normalData[dst + 0] = nx;
            normalData[dst + 1] = ny;
            normalData[dst + 2] = nz;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);

        m_lightData = lightData;
        m_rawLights = std::move(rawLights);
        m_shades    = std::move(shades);
        m_tints     = std::move(tints);
    }

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_positionVbo);
    RenderCommand::uploadArrayBuffer(positionData.data(),
                                     (uint32_t) (positionData.size() * sizeof(uint16_t)),
                                     GL_DYNAMIC_DRAW);

    RenderCommand::bindArrayBuffer(m_uvVbo);
    RenderCommand::uploadArrayBuffer(uvData.data(), (uint32_t) (uvData.size() * sizeof(float)),
                                     GL_DYNAMIC_DRAW);

    RenderCommand::bindArrayBuffer(m_lightVbo);
    RenderCommand::uploadArrayBuffer(lightData.data(), (uint32_t) lightData.size(),
                                     GL_DYNAMIC_DRAW);

    RenderCommand::bindArrayBuffer(m_atlasRectVbo);
    RenderCommand::uploadArrayBuffer(
            atlasRects.data(), (uint32_t) (atlasRects.size() * sizeof(float)), GL_DYNAMIC_DRAW);

    std::vector<uint8_t> tintModeData;
    tintModeData.resize(m_tints.size() * 4);
    for (size_t i = 0; i < m_tints.size(); i++)
    {
        uint32_t packedTint = m_tints[i];
        uint8_t mode        = (uint8_t) ((packedTint >> 24) & 0xFF);
        uint8_t tr          = (uint8_t) ((packedTint >> 16) & 0xFF);
        uint8_t tg          = (uint8_t) ((packedTint >> 8) & 0xFF);
        uint8_t tb          = (uint8_t) (packedTint & 0xFF);

        size_t off            = i * 4;
        tintModeData[off + 0] = tr;
        tintModeData[off + 1] = tg;
        tintModeData[off + 2] = tb;
        tintModeData[off + 3] = mode;
    }

    RenderCommand::bindArrayBuffer(m_tintModeVbo);
    RenderCommand::uploadArrayBuffer(tintModeData.data(), (uint32_t) tintModeData.size(),
                                     GL_DYNAMIC_DRAW);

    std::vector<uint8_t> rawLightData;
    rawLightData.resize(m_rawLights.size() * 4);
    for (size_t i = 0; i < m_rawLights.size(); i++)
    {
        uint16_t raw = m_rawLights[i];

        size_t off            = i * 4;
        rawLightData[off + 0] = packLightNibble((uint8_t) (raw & 15));
        rawLightData[off + 1] = packLightNibble((uint8_t) ((raw >> 4) & 15));
        rawLightData[off + 2] = packLightNibble((uint8_t) ((raw >> 8) & 15));
        rawLightData[off + 3] = packLightNibble((uint8_t) ((raw >> 12) & 15));
    }

    RenderCommand::bindArrayBuffer(m_rawLightVbo);
    RenderCommand::uploadArrayBuffer(rawLightData.data(), (uint32_t) rawLightData.size(),
                                     GL_DYNAMIC_DRAW);

    std::vector<uint8_t> shadeData;
    shadeData.resize(m_shades.size());
    for (size_t i = 0; i < m_shades.size(); i++)
    {
        shadeData[i] = packNormalizedByte(m_shades[i]);
    }

    RenderCommand::bindArrayBuffer(m_shadeVbo);
    RenderCommand::uploadArrayBuffer(shadeData.data(), (uint32_t) shadeData.size(),
                                     GL_DYNAMIC_DRAW);

    RenderCommand::bindArrayBuffer(m_normalVbo);
    RenderCommand::uploadArrayBuffer(normalData.data(), (uint32_t) normalData.size(),
                                     GL_DYNAMIC_DRAW);
}

uint64_t ChunkMesh::getId() const { return m_id; }

void ChunkMesh::snapshotSky(std::vector<uint16_t> *rawLights, std::vector<float> *shades,
                            std::vector<uint32_t> *tints) const
{
    std::lock_guard<std::mutex> lock(m_cpuMutex);
    *rawLights = m_rawLights;
    *shades    = m_shades;
    *tints     = m_tints;
}

void ChunkMesh::applySky(std::vector<uint8_t> &&lightData)
{
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);

        m_lightData = std::move(lightData);
    }

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_lightVbo);
    RenderCommand::bufferSubData(GL_ARRAY_BUFFER, 0, (uint32_t) m_lightData.size(),
                                 m_lightData.data());
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
