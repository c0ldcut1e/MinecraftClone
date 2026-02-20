#include "CloudMesh.h"

#include <glad/glad.h>

#include "../../rendering/RenderCommand.h"

CloudMesh::CloudMesh() : m_vao(0), m_vbo(0), m_vertexCount(0) {
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 7 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 4, GL_FLOAT, false, 7 * sizeof(float), 3 * sizeof(float));
}

CloudMesh::~CloudMesh() {
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void CloudMesh::upload(const std::vector<float> &vertices) {
    m_vertexCount = (uint32_t) (vertices.size() / 7);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(vertices.data(), (uint32_t) (vertices.size() * sizeof(float)), GL_DYNAMIC_DRAW);
}

void CloudMesh::render() const {
    if (!m_vertexCount) return;
    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, (int) m_vertexCount);
}
