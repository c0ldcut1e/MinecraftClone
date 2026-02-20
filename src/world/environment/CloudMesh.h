#pragma once

#include <cstdint>
#include <vector>

class CloudMesh {
public:
    CloudMesh();
    ~CloudMesh();

    void upload(const std::vector<float> &vertices);
    void render() const;

private:
    uint32_t m_vao;
    uint32_t m_vbo;
    uint32_t m_vertexCount;
};
