#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "../../rendering/Texture.h"

class ChunkMesh
{
public:
    ChunkMesh(Texture *texture);
    ~ChunkMesh();

    void upload(const float *vertices, uint32_t vertexCount, std::vector<float> &&atlasRects,
                std::vector<uint16_t> &&rawLights, std::vector<float> &&shades,
                std::vector<uint32_t> &&tints);
    void render() const;

    uint64_t getId() const;
    void snapshotSky(std::vector<uint16_t> *rawLights, std::vector<float> *shades,
                     std::vector<uint32_t> *tints) const;
    void applySky(std::vector<uint8_t> &&lightData);

private:
    Texture *m_texture;
    uint32_t m_vao;
    uint32_t m_positionVbo;
    uint32_t m_uvVbo;
    uint32_t m_lightVbo;
    uint32_t m_atlasRectVbo;
    uint32_t m_tintModeVbo;
    uint32_t m_rawLightVbo;
    uint32_t m_shadeVbo;
    uint32_t m_normalVbo;
    uint32_t m_vertexCount;

    uint64_t m_id;

    mutable std::mutex m_cpuMutex;
    std::vector<uint8_t> m_lightData;
    std::vector<uint16_t> m_rawLights;
    std::vector<float> m_shades;
    std::vector<uint32_t> m_tints;
};
