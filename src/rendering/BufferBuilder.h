#pragma once

#include <cstdint>
#include <vector>

#include "../utils/math/Mat4.h"
#include "Shader.h"
#include "Texture.h"

class Tesselator;

class BufferBuilder
{
public:
    void begin(uint32_t mode);
    void color(uint32_t argb);

    void vertex(float x, float y, float z);
    void vertexUV(float x, float y, float z, float u, float v);

    void bindTexture(Texture *texture);
    void unbindTexture();
    void setAlphaCutoff(float cutoff);

    void setViewProjection(const Mat4 &view, const Mat4 &projection);
    void setScreenProjection(const Mat4 &projection);

    void end();

private:
    friend class Tesselator;

    explicit BufferBuilder(uint16_t type);
    ~BufferBuilder();

    struct Vertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    std::vector<Vertex> m_vertices;

    uint32_t m_vao;
    uint32_t m_vbo;

    uint32_t m_mode;

    float m_currentColor[4];

    Texture *m_texture;
    float m_alphaCutoff;

    Shader m_shader;
    uint16_t m_type;

    Mat4 m_view;
    Mat4 m_projection;
    Mat4 m_screenProjection;
};
