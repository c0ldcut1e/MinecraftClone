#include "UIComponent_Crosshair.h"

#include <GL/glew.h>

#include "../core/Minecraft.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"

UIComponent_Crosshair::UIComponent_Crosshair()
    : UIComponent("ComponentCrosshair"), m_shader("shaders/crosshair.vert", "shaders/crosshair.frag"), m_vao(0), m_vbo(0), m_captureTexture(0), m_captureSize(0), m_alpha(0.65f), m_thickness(0.09f), m_gap(0.0f), m_arm(0.8f), m_size(48.0f) {
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::uploadArrayBuffer(nullptr, (uint32_t) (6 * 5 * sizeof(float)), GL_DYNAMIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(float), 3 * sizeof(float));

    m_captureTexture = RenderCommand::createTexture();
    RenderCommand::bindTexture2D(m_captureTexture);

    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ensureCaptureTexture((int) m_size + 2);

    m_shader.bind();
    m_shader.setInt("u_capture", 0);
}

UIComponent_Crosshair::~UIComponent_Crosshair() {
    if (m_captureTexture) RenderCommand::deleteTexture(m_captureTexture);

    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void UIComponent_Crosshair::tick() {}

void UIComponent_Crosshair::render() {
    Minecraft *minecraft = Minecraft::getInstance();

    int width  = minecraft->getWidth();
    int height = minecraft->getHeight();

    ensureCaptureTexture((int) m_size + 2);

    int centerX = width / 2;
    int centerY = height / 2;

    int halfCapture = m_captureSize / 2;

    int srcX = centerX - halfCapture;
    int srcY = (height - centerY) - halfCapture;

    if (srcX < 0) srcX = 0;
    if (srcY < 0) srcY = 0;
    if (srcX + m_captureSize > width) srcX = width - m_captureSize;
    if (srcY + m_captureSize > height) srcY = height - m_captureSize;
    if (srcX < 0) srcX = 0;
    if (srcY < 0) srcY = 0;

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_captureTexture);
    RenderCommand::copyTexSubImage2D(srcX, srcY, m_captureSize, m_captureSize);

    float halfSize = m_size * 0.5f;

    float x0 = (float) centerX - halfSize;
    float y0 = (float) centerY - halfSize;
    float x1 = (float) centerX + halfSize;
    float y1 = (float) centerY + halfSize;

    float vertices[6][5] = {
            {x0, y0, 0.0f, 0.0f, 0.0f}, {x1, y0, 0.0f, 1.0f, 0.0f}, {x1, y1, 0.0f, 1.0f, 1.0f}, {x0, y0, 0.0f, 0.0f, 0.0f}, {x1, y1, 0.0f, 1.0f, 1.0f}, {x0, y1, 0.0f, 0.0f, 1.0f},
    };

    GlStateManager::disableDepthTest();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), GL_DYNAMIC_DRAW);

    m_shader.bind();
    m_shader.setVec2("u_resolution", (float) width, (float) height);
    m_shader.setFloat("u_alpha", m_alpha);
    m_shader.setFloat("u_thickness", m_thickness);
    m_shader.setFloat("u_gap", m_gap);
    m_shader.setFloat("u_arm", m_arm);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 6);

    GlStateManager::disableBlend();
    GlStateManager::enableDepthTest();
}

void UIComponent_Crosshair::ensureCaptureTexture(int captureSize) {
    if (captureSize < 1) captureSize = 1;
    if (m_captureSize == captureSize) return;

    m_captureSize = captureSize;

    RenderCommand::bindTexture2D(m_captureTexture);
    RenderCommand::uploadTexture2D(m_captureSize, m_captureSize, RC_RGBA8, RC_RGBA, RC_UNSIGNED_BYTE, nullptr);
}
