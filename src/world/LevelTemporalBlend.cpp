#include "LevelTemporalBlend.h"

#include <cmath>

#include <glad/glad.h>

#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"
#include "../utils/math/Mth.h"

LevelTemporalBlend::LevelTemporalBlend(int width, int height)
    : m_shader(nullptr), m_targets{nullptr, nullptr}, m_fullscreenVao(0), m_writeIndex(0),
      m_historyIndex(0), m_hasHistory(false), m_previousCameraPos(),
      m_settings{0.16f, 0.35f, 0.00075f, 0.015f}
{
    m_shader        = new Shader("shaders/level_blend.vert", "shaders/level_blend.frag");
    m_targets[0]    = new Framebuffer(width, height);
    m_targets[1]    = new Framebuffer(width, height);
    m_fullscreenVao = RenderCommand::createVertexArray();
}

LevelTemporalBlend::~LevelTemporalBlend()
{
    if (m_fullscreenVao)
    {
        RenderCommand::deleteVertexArray(m_fullscreenVao);
        m_fullscreenVao = 0;
    }

    if (m_targets[1])
    {
        delete m_targets[1];
        m_targets[1] = nullptr;
    }

    if (m_targets[0])
    {
        delete m_targets[0];
        m_targets[0] = nullptr;
    }

    if (m_shader)
    {
        delete m_shader;
        m_shader = nullptr;
    }
}

void LevelTemporalBlend::resize(int width, int height)
{
    if (m_targets[0])
    {
        m_targets[0]->resize(width, height);
    }
    if (m_targets[1])
    {
        m_targets[1]->resize(width, height);
    }

    reset();
}

void LevelTemporalBlend::reset()
{
    m_writeIndex        = 0;
    m_historyIndex      = 0;
    m_hasHistory        = false;
    m_previousCameraPos = Vec3();
}

Framebuffer *LevelTemporalBlend::beginWrite()
{
    m_writeIndex = m_hasHistory ? 1 - m_historyIndex : 0;

    Framebuffer *target = m_targets[m_writeIndex];
    target->bind();
    GlStateManager::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    GlStateManager::clearAll();
    return target;
}

void LevelTemporalBlend::compositeToScene(const Framebuffer &sceneFramebuffer,
                                          const Vec3 &cameraPos)
{
    Framebuffer *current = m_targets[m_writeIndex];
    Framebuffer *history = m_hasHistory ? m_targets[m_historyIndex] : nullptr;

    copyDepthToScene(*current, sceneFramebuffer);
    sceneFramebuffer.bind();

    GlStateManager::disableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->bind();

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(current->getColorTexture());
    m_shader->setInt("u_currentColor", 0);

    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(current->getDepthTexture());
    m_shader->setInt("u_currentDepth", 1);

    RenderCommand::activeTexture(2);
    RenderCommand::bindTexture2D(history ? history->getColorTexture() : current->getColorTexture());
    m_shader->setInt("u_historyColor", 2);

    RenderCommand::activeTexture(3);
    RenderCommand::bindTexture2D(history ? history->getDepthTexture() : current->getDepthTexture());
    m_shader->setInt("u_historyDepth", 3);

    m_shader->setInt("u_hasHistory", history ? 1 : 0);
    m_shader->setFloat("u_historyWeight", history ? getHistoryWeight(cameraPos) : 0.0f);
    m_shader->setFloat("u_depthTolerance", m_settings.depthTolerance);
    m_shader->setFloat("u_depthFadeRange", m_settings.depthFadeRange);

    RenderCommand::bindVertexArray(m_fullscreenVao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);
    RenderCommand::bindVertexArray(0);

    GlStateManager::disableBlend();
    GlStateManager::setDepthMask(true);
    GlStateManager::enableDepthTest();
    GlStateManager::enableCull();

    m_historyIndex      = m_writeIndex;
    m_hasHistory        = true;
    m_previousCameraPos = cameraPos;
}

const Framebuffer *LevelTemporalBlend::getCurrentTarget() const
{
    if (m_hasHistory)
    {
        return m_targets[m_historyIndex];
    }

    return m_targets[m_writeIndex];
}

LevelTemporalBlend::Settings &LevelTemporalBlend::getSettings() { return m_settings; }

const LevelTemporalBlend::Settings &LevelTemporalBlend::getSettings() const { return m_settings; }

float LevelTemporalBlend::getHistoryWeight(const Vec3 &cameraPos) const
{
    if (!m_hasHistory || m_settings.motionFadeDistance <= 0.0f)
    {
        return 0.0f;
    }

    float motion = (float) cameraPos.sub(m_previousCameraPos).length();
    float fade   = 1.0f - Mth::clampf(motion / m_settings.motionFadeDistance, 0.0f, 1.0f);
    return m_settings.historyWeight * fade * fade;
}

void LevelTemporalBlend::copyDepthToScene(const Framebuffer &source,
                                          const Framebuffer &sceneFramebuffer) const
{
    RenderCommand::bindFramebuffer(GL_READ_FRAMEBUFFER, source.getId());
    RenderCommand::bindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFramebuffer.getId());

    glBlitFramebuffer(0, 0, source.getWidth(), source.getHeight(), 0, 0,
                      sceneFramebuffer.getWidth(), sceneFramebuffer.getHeight(),
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    RenderCommand::bindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer.getId());
}
