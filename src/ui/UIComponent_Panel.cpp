#include "UIComponent_Panel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/BufferBuilder.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/Tesselator.h"
#include "../utils/math/Mth.h"
#include "UIPanelProperties.h"
#include "UIScreen.h"

static uint32_t packPanelColor(float transparency)
{
    uint32_t alpha = (uint32_t) (Mth::clampf(transparency, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (alpha << 24) | 0x00FFFFFFu;
}

static void emitPanelQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1, float u0,
                          float v0, float u1, float v1, uint32_t color)
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

UIComponent_Panel::UIComponent_Panel(std::unique_ptr<UIPanelProperties> properties)
    : UIComponent("ComponentPanel"), m_properties(std::move(properties)),
      m_texture(m_properties ? m_properties->getTexturePath() : "textures/ui/panel9grid.png")
{}

UIComponent_Panel::~UIComponent_Panel() {}

void UIComponent_Panel::render()
{
    if (!m_properties || m_properties->width <= 0.0f || m_properties->height <= 0.0f ||
        m_properties->transparency <= 0.0f)
    {
        return;
    }

    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    int actualWidth   = minecraft->getWidth();
    int actualHeight  = minecraft->getHeight();
    int textureWidth  = m_texture.getPixelWidth();
    int textureHeight = m_texture.getPixelHeight();
    if (textureWidth <= 0 || textureHeight <= 0)
    {
        return;
    }

    float panelX      = UIScreen::toActualX(m_properties->x, actualWidth);
    float panelY      = UIScreen::toActualY(m_properties->y, actualWidth, actualHeight);
    float panelWidth  = UIScreen::toActualLength(m_properties->width, actualWidth, actualHeight);
    float panelHeight = UIScreen::toActualLength(m_properties->height, actualWidth, actualHeight);

    constexpr float panelSliceRatio = 12.0f / 96.0f;
    float sourceSliceX = std::max(1.0f, (float) std::round((float) textureWidth * panelSliceRatio));
    float sourceSliceY =
            std::max(1.0f, (float) std::round((float) textureHeight * panelSliceRatio));
    float sliceX = UIScreen::toActualLength(sourceSliceX, actualWidth, actualHeight);
    float sliceY = UIScreen::toActualLength(sourceSliceY, actualWidth, actualHeight);

    float left   = std::min(sliceX, panelWidth * 0.5f);
    float right  = std::min(sliceX, panelWidth - left);
    float top    = std::min(sliceY, panelHeight * 0.5f);
    float bottom = std::min(sliceY, panelHeight - top);

    float x0 = panelX;
    float x1 = panelX + left;
    float x2 = panelX + panelWidth - right;
    float x3 = panelX + panelWidth;

    float y0 = panelY;
    float y1 = panelY + top;
    float y2 = panelY + panelHeight - bottom;
    float y3 = panelY + panelHeight;

    float u0 = 0.0f;
    float u1 = sourceSliceX / (float) textureWidth;
    float u2 = ((float) textureWidth - sourceSliceX) / (float) textureWidth;
    float u3 = 1.0f;

    float v0 = 0.0f;
    float v1 = sourceSliceY / (float) textureHeight;
    float v2 = ((float) textureHeight - sourceSliceY) / (float) textureHeight;
    float v3 = 1.0f;

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();
    uint32_t color         = packPanelColor(m_properties->transparency);

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(&m_texture);
    emitPanelQuad(builder, x0, y0, x1, y1, u0, v0, u1, v1, color);
    emitPanelQuad(builder, x1, y0, x2, y1, u1, v0, u2, v1, color);
    emitPanelQuad(builder, x2, y0, x3, y1, u2, v0, u3, v1, color);
    emitPanelQuad(builder, x0, y1, x1, y2, u0, v1, u1, v2, color);
    emitPanelQuad(builder, x1, y1, x2, y2, u1, v1, u2, v2, color);
    emitPanelQuad(builder, x2, y1, x3, y2, u2, v1, u3, v2, color);
    emitPanelQuad(builder, x0, y2, x1, y3, u0, v2, u1, v3, color);
    emitPanelQuad(builder, x1, y2, x2, y3, u1, v2, u2, v3, color);
    emitPanelQuad(builder, x2, y2, x3, y3, u2, v2, u3, v3, color);
    builder->end();
    builder->unbindTexture();

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();
}

UIPanelProperties *UIComponent_Panel::getProperties() { return m_properties.get(); }

const UIPanelProperties *UIComponent_Panel::getProperties() const { return m_properties.get(); }
