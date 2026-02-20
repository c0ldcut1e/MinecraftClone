#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "../../rendering/Texture.h"

class ChunkMesh {
public:
    ChunkMesh(Texture *texture);
    ~ChunkMesh();

    void upload(const float *vertices, uint32_t vertexCount, std::vector<uint16_t> &&rawLights, std::vector<float> &&shades, std::vector<uint32_t> &&tints);
    void render() const;

    uint64_t getId() const;
    void snapshotSky(std::vector<float> &vertices, std::vector<uint16_t> &rawLights, std::vector<float> &shades, std::vector<uint32_t> &tints) const;
    void applySky(std::vector<float> &&vertices);

private:
    Texture *m_texture;
    uint32_t m_vao;
    uint32_t m_vbo;
    uint32_t m_vertexCount;

    uint64_t m_id;

    mutable std::mutex m_cpuMutex;
    std::vector<float> m_vertices;
    std::vector<uint16_t> m_rawLights;
    std::vector<float> m_shades;
    std::vector<uint32_t> m_tints;
};
