#include "UIControl_ProgressBar.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glad/glad.h>

#include "../../core/Minecraft.h"
#include "../../rendering/BufferBuilder.h"
#include "../../rendering/GlStateManager.h"
#include "../../rendering/Tesselator.h"
#include "../../utils/math/Mth.h"

#define PROGRESS_BAR_DEFAULT_WIDTH        900.0f
#define PROGRESS_BAR_DEFAULT_HEIGHT       40.0f
#define PROGRESS_BAR_SOURCE_BORDER_PIXELS 2.0f

static void emitProgressBarQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1,
                                float u0, float v0, float u1, float v1, uint32_t color)
{
    if (!builder || x1 <= x0 || y1 <= y0)
    {
        return;
    }

    builder->color(color);
    builder->vertexUV(x0, y0, 0.0f, u0, v0);
    builder->vertexUV(x1, y0, 0.0f, u1, v0);
    builder->vertexUV(x1, y1, 0.0f, u1, v1);
    builder->vertexUV(x0, y0, 0.0f, u0, v0);
    builder->vertexUV(x1, y1, 0.0f, u1, v1);
    builder->vertexUV(x0, y1, 0.0f, u0, v1);
}

static void emitProgressBarNineSlice(BufferBuilder *builder, float x, float y, float width,
                                     float height, float textureWidth, float textureHeight,
                                     float sourceBorderX, float sourceBorderY,
                                     float destinationBorderX, float destinationBorderY,
                                     uint32_t color)
{
    if (!builder || width <= 0.0f || height <= 0.0f || textureWidth <= 0.0f ||
        textureHeight <= 0.0f)
    {
        return;
    }

    float left   = std::min(destinationBorderX, width * 0.5f);
    float right  = std::min(destinationBorderX, width - left);
    float top    = std::min(destinationBorderY, height * 0.5f);
    float bottom = std::min(destinationBorderY, height - top);

    float x0 = x;
    float x1 = x + left;
    float x2 = x + width - right;
    float x3 = x + width;

    float y0 = y;
    float y1 = y + top;
    float y2 = y + height - bottom;
    float y3 = y + height;

    float u0 = 0.0f;
    float u1 = sourceBorderX / textureWidth;
    float u2 = (textureWidth - sourceBorderX) / textureWidth;
    float u3 = 1.0f;

    float v0 = 0.0f;
    float v1 = sourceBorderY / textureHeight;
    float v2 = (textureHeight - sourceBorderY) / textureHeight;
    float v3 = 1.0f;

    emitProgressBarQuad(builder, x0, y0, x1, y1, u0, v0, u1, v1, color);
    emitProgressBarQuad(builder, x1, y0, x2, y1, u1, v0, u2, v1, color);
    emitProgressBarQuad(builder, x2, y0, x3, y1, u2, v0, u3, v1, color);
    emitProgressBarQuad(builder, x0, y1, x1, y2, u0, v1, u1, v2, color);
    emitProgressBarQuad(builder, x1, y1, x2, y2, u1, v1, u2, v2, color);
    emitProgressBarQuad(builder, x2, y1, x3, y2, u2, v1, u3, v2, color);
    emitProgressBarQuad(builder, x0, y2, x1, y3, u0, v2, u1, v3, color);
    emitProgressBarQuad(builder, x1, y2, x2, y3, u1, v2, u2, v3, color);
    emitProgressBarQuad(builder, x2, y2, x3, y3, u2, v2, u3, v3, color);
}

UIControl_ProgressBar::UIControl_ProgressBar(float x, float y, float width, float height,
                                             float progress)
    : UIControl("Control_ProgressBar", x, y, width, height), m_progress(0.0f),
      m_backgroundTexture("textures/ui/progressbar_background.png"),
      m_fillerTexture("textures/ui/progressbar_filler.png")
{
    setProgress(progress);
}

UIControl_ProgressBar::~UIControl_ProgressBar() {}

void UIControl_ProgressBar::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    int actualWidth     = minecraft->getWidth();
    int actualHeight    = minecraft->getHeight();
    UIScreen::Rect rect = getActualRect(actualWidth, actualHeight);

    float roundedX      = std::round(rect.x);
    float roundedY      = std::round(rect.y);
    float roundedWidth  = std::round(rect.width);
    float roundedHeight = std::round(rect.height);

    if (roundedWidth <= 0.0f || roundedHeight <= 0.0f)
    {
        return;
    }

    float backgroundPixelHeight =
            std::max(1.0f, (float) std::max(m_backgroundTexture.getPixelHeight(), 1));
    float backgroundPixelScale = std::max(1.0f, std::round(roundedHeight / backgroundPixelHeight));
    float backgroundBorderSize = backgroundPixelScale * PROGRESS_BAR_SOURCE_BORDER_PIXELS;

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(&m_backgroundTexture);
    builder->setAlphaCutoff(0.01f);
    emitProgressBarNineSlice(builder, roundedX, roundedY, roundedWidth, roundedHeight,
                             (float) std::max(m_backgroundTexture.getPixelWidth(), 1),
                             backgroundPixelHeight, PROGRESS_BAR_SOURCE_BORDER_PIXELS,
                             PROGRESS_BAR_SOURCE_BORDER_PIXELS, backgroundBorderSize,
                             backgroundBorderSize, 0xFFFFFFFFu);
    builder->end();
    builder->unbindTexture();

    float fillerX      = roundedX + backgroundBorderSize;
    float fillerY      = roundedY + backgroundBorderSize;
    float fillerWidth  = std::max(0.0f, roundedWidth - backgroundBorderSize * 2.0f);
    float fillerHeight = std::max(0.0f, roundedHeight - backgroundBorderSize * 2.0f);
    float filledWidth  = std::round(fillerWidth * m_progress);

    if (m_progress > 0.0f && filledWidth <= 0.0f)
    {
        filledWidth = 1.0f;
    }

    if (filledWidth > 0.0f && fillerHeight > 0.0f)
    {
        builder->begin(GL_TRIANGLES);
        builder->bindTexture(&m_fillerTexture);
        builder->setAlphaCutoff(0.01f);
        emitProgressBarQuad(
                builder, fillerX, fillerY, fillerX + filledWidth, fillerY + fillerHeight, 0.0f,
                0.0f, fillerWidth > 0.0f ? filledWidth / fillerWidth : 0.0f, 1.0f, 0xFFFFFFFFu);
        builder->end();
        builder->unbindTexture();
    }

    builder->setAlphaCutoff(-1.0f);

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();
}

float UIControl_ProgressBar::getPreferredWidth() const
{
    return std::max(m_width, PROGRESS_BAR_DEFAULT_WIDTH);
}

float UIControl_ProgressBar::getPreferredHeight() const
{
    return std::max(m_height, PROGRESS_BAR_DEFAULT_HEIGHT);
}

bool UIControl_ProgressBar::isSelectable() const { return false; }

bool UIControl_ProgressBar::setProgress(float progress)
{
    float clampedProgress = Mth::clampf(progress, 0.0f, 1.0f);
    if (fabsf(m_progress - clampedProgress) < 0.0001f)
    {
        return false;
    }

    m_progress = clampedProgress;
    return true;
}

float UIControl_ProgressBar::getProgress() const { return m_progress; }
