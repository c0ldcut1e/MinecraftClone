#pragma once

#include <cstdint>
#include <vector>

#include "../utils/math/Mat4.h"
#include "Shader.h"
#include "Texture.h"

class ImmediateRenderer {
public:
    static ImmediateRenderer *getForScreen();
    static ImmediateRenderer *getForWorld();

    void begin(uint32_t mode);
    void color(uint32_t argb);

    void vertex(float x, float y, float z);
    void vertexUV(float x, float y, float z, float u, float v);

    void bindTexture(Texture *texture);
    void unbindTexture();

    void setViewProjection(const Mat4 &view, const Mat4 &projection);
    void setScreenSize(int width, int height);

    void end();

private:
    ImmediateRenderer(uint16_t type);
    ~ImmediateRenderer();

    Mat4 createProjection();

    struct Vertex {
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

    Shader m_shader;
    uint16_t m_type;

    Mat4 m_view;
    Mat4 m_projection;

    int m_screenWidth;
    int m_screenHeight;
};
