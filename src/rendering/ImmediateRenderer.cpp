#include "ImmediateRenderer.h"

#include <glad/glad.h>

#include "GlStateManager.h"
#include "RenderCommand.h"

ImmediateRenderer *ImmediateRenderer::getForScreen() {
    static ImmediateRenderer instance(0);
    return &instance;
}

ImmediateRenderer *ImmediateRenderer::getForWorld() {
    static ImmediateRenderer instance(1);
    return &instance;
}

ImmediateRenderer::ImmediateRenderer(uint16_t type)
    : m_vao(0), m_vbo(0), m_mode(GL_TRIANGLES), m_texture(nullptr), m_shader("shaders/immediate.vert", "shaders/immediate.frag"), m_type(type), m_view(Mat4::identity()), m_projection(Mat4::identity()),
      m_screenProjection(Mat4::orthographic(0.0, 1920.0, 1080.0, 0.0, -1.0, 1.0)) {
    m_currentColor[0] = 1.0f;
    m_currentColor[1] = 1.0f;
    m_currentColor[2] = 1.0f;
    m_currentColor[3] = 1.0f;

    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(Vertex), 3 * sizeof(float));

    RenderCommand::enableVertexAttrib(2);
    RenderCommand::setVertexAttribPointer(2, 4, GL_FLOAT, false, sizeof(Vertex), 5 * sizeof(float));
}

ImmediateRenderer::~ImmediateRenderer() {
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void ImmediateRenderer::begin(uint32_t mode) {
    m_mode = mode;
    m_vertices.clear();
}

void ImmediateRenderer::color(uint32_t argb) {
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >> 8) & 0xFF) / 255.0f;
    float b = (argb & 0xFF) / 255.0f;

    m_currentColor[0] = r;
    m_currentColor[1] = g;
    m_currentColor[2] = b;
    m_currentColor[3] = a;
}

void ImmediateRenderer::vertex(float x, float y, float z) { vertexUV(x, y, z, 0.0f, 0.0f); }

void ImmediateRenderer::vertexUV(float x, float y, float z, float u, float v) {
    Vertex vertex;
    vertex.x = x;
    vertex.y = y;
    vertex.z = z;
    vertex.u = u;
    vertex.v = v;
    vertex.r = m_currentColor[0];
    vertex.g = m_currentColor[1];
    vertex.b = m_currentColor[2];
    vertex.a = m_currentColor[3];
    m_vertices.push_back(vertex);
}

void ImmediateRenderer::bindTexture(Texture *texture) { m_texture = texture; }

void ImmediateRenderer::unbindTexture() { m_texture = nullptr; }

void ImmediateRenderer::setViewProjection(const Mat4 &view, const Mat4 &projection) {
    m_view       = view;
    m_projection = projection;
}

void ImmediateRenderer::setScreenProjection(const Mat4 &projection) { m_screenProjection = projection; }

void ImmediateRenderer::end() {
    if (m_vertices.empty()) return;

    GlStateManager::disableCull();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(m_vertices.data(), (uint32_t) (m_vertices.size() * sizeof(Vertex)), GL_DYNAMIC_DRAW);

    m_shader.bind();

    Mat4 finalView       = m_view;
    Mat4 finalProjection = m_projection;
    Mat4 finalModel      = GlStateManager::getMatrix();

    if (m_type == 0) {
        finalView       = Mat4::identity();
        finalProjection = m_screenProjection;
        finalModel      = Mat4::identity();
    }

    m_shader.setMat4("u_view", finalView.data);
    m_shader.setMat4("u_projection", finalProjection.data);
    m_shader.setMat4("u_model", finalModel.data);
    m_shader.setInt("u_texture", 0);

    if (m_texture) {
        m_shader.setInt("u_useTexture", 1);
        m_texture->bind(0);
    } else
        m_shader.setInt("u_useTexture", 0);

    RenderCommand::renderArrays(m_mode, 0, (int) m_vertices.size());

    GlStateManager::enableCull();
}
