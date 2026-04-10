#include "UIControl_Button.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glad/glad.h>

#include <SDL2/SDL.h>

#include "../../core/Minecraft.h"
#include "../../rendering/BufferBuilder.h"
#include "../../rendering/Font.h"
#include "../../rendering/GlStateManager.h"
#include "../../rendering/Tesselator.h"
#include "../UIScreen.h"

#define BUTTON_DEFAULT_WIDTH       600.0f
#define BUTTON_DEFAULT_HEIGHT      50.0f
#define BUTTON_TEXT_SCALE          0.95f
#define BUTTON_SELECTED_TEXT_COLOR 0xFFFFFF00
#define BUTTON_DEFAULT_TEXT_COLOR  0xFFFFFFFF

static void emitButtonQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1, float u0,
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

UIControl_Button::UIControl_Button(float x, float y, float width, float height,
                                   const std::wstring &label)
    : UIControl_Button(x, y, width, height, label, "textures/ui/listbutton.png",
                       "textures/ui/listbutton_highlight.png", BUTTON_DEFAULT_HEIGHT,
                       BUTTON_DEFAULT_WIDTH)
{}

UIControl_Button::~UIControl_Button() {}

void UIControl_Button::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    Font *font = minecraft->getDefaultFont();
    if (!font)
    {
        return;
    }

    int actualWidth     = minecraft->getWidth();
    int actualHeight    = minecraft->getHeight();
    UIScreen::Rect rect = getActualRect(actualWidth, actualHeight);
    float uiScale       = UIScreen::scaleUniform(actualWidth, actualHeight);

    float displayHeight = std::round(rect.height);
    float displayWidth;
    if (m_preferredWidth > 0.0f)
    {
        displayWidth =
                std::round(UIScreen::toActualLength(m_preferredWidth, actualWidth, actualHeight));
    }
    else
    {
        int pixelWidth  = m_normalTexture.getPixelWidth();
        int pixelHeight = std::max(m_normalTexture.getPixelHeight(), 1);
        displayWidth    = std::round(displayHeight * ((float) pixelWidth / (float) pixelHeight));
        if (displayWidth <= 0.0f)
        {
            displayWidth = std::round(
                    UIScreen::toActualLength(BUTTON_DEFAULT_WIDTH, actualWidth, actualHeight));
        }
    }

    float btnX = std::round(rect.x + (rect.width - displayWidth) * 0.5f);
    float btnY = std::round(rect.y);

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();
    Texture *texture       = m_selected ? &m_highlightTexture : &m_normalTexture;

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(texture);
    builder->setAlphaCutoff(0.01f);
    emitButtonQuad(builder, btnX, btnY, btnX + displayWidth, btnY + displayHeight, 0.0f, 0.0f, 1.0f,
                   1.0f, 0xFFFFFFFFu);
    builder->end();
    builder->unbindTexture();
    builder->setAlphaCutoff(-1.0f);

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();

    if (!m_label.empty())
    {
        float fontScale = font->snapScale(uiScale * BUTTON_TEXT_SCALE);
        float textWidth = font->getWidth(m_label, fontScale);
        float textX     = std::round(btnX + (displayWidth - textWidth) * 0.5f);
        float textBaselineY =
                std::round(btnY + (displayHeight - font->getLineHeight(fontScale)) * 0.5f +
                           font->getAscent(fontScale));
        bool flashing = m_wasPressed && (SDL_GetTicks() - m_pressedAtMs) < 500u;
        uint32_t textColor =
                (flashing || !m_selected) ? BUTTON_DEFAULT_TEXT_COLOR : BUTTON_SELECTED_TEXT_COLOR;

        font->setNearest(true);
        font->draw(m_label, textX + 1.0f, textBaselineY + 1.0f, fontScale, 0xFF000000);
        font->draw(m_label, textX, textBaselineY, fontScale, textColor);
    }
}

float UIControl_Button::getPreferredWidth() const
{
    if (m_preferredWidth > 0.0f)
    {
        return m_preferredWidth;
    }

    int pixelWidth  = m_normalTexture.getPixelWidth();
    int pixelHeight = m_normalTexture.getPixelHeight();
    if (pixelWidth <= 0 || pixelHeight <= 0)
    {
        return m_width > 0.0f ? m_width : BUTTON_DEFAULT_WIDTH;
    }

    return m_preferredHeight * ((float) pixelWidth / (float) pixelHeight);
}

float UIControl_Button::getPreferredHeight() const { return std::max(m_height, m_preferredHeight); }

void UIControl_Button::activate()
{
    m_wasPressed  = true;
    m_pressedAtMs = SDL_GetTicks();
}

bool UIControl_Button::onPointerPressed(int, int, int, int)
{
    m_wasPressed  = true;
    m_pressedAtMs = SDL_GetTicks();
    return true;
}

const std::wstring &UIControl_Button::getLabel() const { return m_label; }

void UIControl_Button::setLabel(const std::wstring &label) { m_label = label; }

UIControl_Button::UIControl_Button(float x, float y, float width, float height,
                                   const std::wstring &label, const char *normalTexturePath,
                                   const char *highlightTexturePath, float preferredHeight,
                                   float preferredWidth)
    : UIControl("Control_Button", x, y, width, height), m_label(label),
      m_normalTexture(normalTexturePath), m_highlightTexture(highlightTexturePath),
      m_preferredHeight(std::max(preferredHeight, BUTTON_DEFAULT_HEIGHT)),
      m_preferredWidth(std::max(preferredWidth, 0.0f)), m_wasPressed(false), m_pressedAtMs(0)
{}
