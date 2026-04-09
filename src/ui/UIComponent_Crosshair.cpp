#include "UIComponent_Crosshair.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"
#include "UIScreen.h"

UIComponent_Crosshair::UIComponent_Crosshair()
    : UIComponent("ComponentCrosshair"),
      m_shader("shaders/crosshair.vert", "shaders/crosshair.frag"), m_vao(0), m_vbo(0),
      m_captureTexture(0), m_captureWidth(0), m_captureHeight(0), m_alpha(0.65f),
      m_thickness(0.09f), m_gap(0.0f), m_arm(0.8f), m_size(30.0f)
{
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::uploadArrayBuffer(nullptr, (uint32_t) (6 * 5 * sizeof(float)), GL_DYNAMIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(float),
                                          3 * sizeof(float));

    m_captureTexture = RenderCommand::createTexture();
    RenderCommand::bindTexture2D(m_captureTexture);

    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ensureCaptureTexture(1, 1);

    m_shader.bind();
    m_shader.setInt("u_capture", 0);
}

UIComponent_Crosshair::~UIComponent_Crosshair()
{
    if (m_captureTexture)
    {
        RenderCommand::deleteTexture(m_captureTexture);
    }

    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void UIComponent_Crosshair::tick() {}

void UIComponent_Crosshair::render()
{
    Minecraft *minecraft = Minecraft::getInstance();

    int actualWidth  = minecraft->getWidth();
    int actualHeight = minecraft->getHeight();
    float uiScale    = UIScreen::scaleUniform(actualWidth, actualHeight);
    float width      = (float) actualWidth;
    float height     = (float) actualHeight;

    int captureWidth  = std::max(1, (int) lroundf(m_size * uiScale));
    int captureHeight = std::max(1, (int) lroundf(m_size * uiScale));
    ensureCaptureTexture(captureWidth, captureHeight);

    int actualCenterX = (int) lroundf(UIScreen::toActualX(UIScreen::WIDTH * 0.5f, actualWidth));
    int actualCenterY = (int) lroundf(UIScreen::toActualY(UIScreen::HEIGHT * 0.5f, actualHeight));

    int halfCaptureWidth  = m_captureWidth / 2;
    int halfCaptureHeight = m_captureHeight / 2;

    int srcX = actualCenterX - halfCaptureWidth;
    int srcY = (actualHeight - actualCenterY) - halfCaptureHeight;

    if (srcX < 0)
    {
        srcX = 0;
    }
    if (srcY < 0)
    {
        srcY = 0;
    }
    if (srcX + m_captureWidth > actualWidth)
    {
        srcX = actualWidth - m_captureWidth;
    }
    if (srcY + m_captureHeight > actualHeight)
    {
        srcY = actualHeight - m_captureHeight;
    }
    if (srcX < 0)
    {
        srcX = 0;
    }
    if (srcY < 0)
    {
        srcY = 0;
    }

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_captureTexture);
    RenderCommand::copyTexSubImage2D(srcX, srcY, m_captureWidth, m_captureHeight);

    float halfSize = m_size * uiScale * 0.5f;

    float centerX = UIScreen::toActualX(UIScreen::WIDTH * 0.5f, actualWidth);
    float centerY = UIScreen::toActualY(UIScreen::HEIGHT * 0.5f, actualHeight);
    float x0      = centerX - halfSize;
    float y0      = centerY - halfSize;
    float x1      = centerX + halfSize;
    float y1      = centerY + halfSize;

    float vertices[6][5] = {
            {x0, y0, 0.0f, 0.0f, 0.0f}, {x1, y0, 0.0f, 1.0f, 0.0f}, {x1, y1, 0.0f, 1.0f, 1.0f},
            {x0, y0, 0.0f, 0.0f, 0.0f}, {x1, y1, 0.0f, 1.0f, 1.0f}, {x0, y1, 0.0f, 0.0f, 1.0f},
    };

    GlStateManager::disableDepthTest();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), GL_DYNAMIC_DRAW);

    m_shader.bind();
    m_shader.setVec2("u_resolution", width, height);
    m_shader.setFloat("u_alpha", m_alpha);
    m_shader.setFloat("u_thickness", m_thickness);
    m_shader.setFloat("u_gap", m_gap);
    m_shader.setFloat("u_arm", m_arm);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 6);

    GlStateManager::disableBlend();
    GlStateManager::enableDepthTest();
}

void UIComponent_Crosshair::ensureCaptureTexture(int captureWidth, int captureHeight)
{
    if (captureWidth < 1)
    {
        captureWidth = 1;
    }
    if (captureHeight < 1)
    {
        captureHeight = 1;
    }
    if (m_captureWidth == captureWidth && m_captureHeight == captureHeight)
    {
        return;
    }

    m_captureWidth  = captureWidth;
    m_captureHeight = captureHeight;

    RenderCommand::bindTexture2D(m_captureTexture);
    RenderCommand::uploadTexture2D(m_captureWidth, m_captureHeight, GL_RGBA8, GL_RGBA,
                                   GL_UNSIGNED_BYTE, nullptr);
}
