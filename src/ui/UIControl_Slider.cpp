#include "UIControl_Slider.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/BufferBuilder.h"
#include "../rendering/Font.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/Tesselator.h"
#include "../utils/math/Mth.h"
#include "UIScreen.h"

static constexpr float SLIDER_DEFAULT_WIDTH             = 300.0f;
static constexpr float SLIDER_DEFAULT_HEIGHT            = 36.0f;
static constexpr float SLIDER_TEXT_SCALE                = 0.78f;
static constexpr uint32_t SLIDER_SELECTED_TEXT_COLOR    = 0xFFFFFF00;
static constexpr uint32_t SLIDER_DEFAULT_TEXT_COLOR     = 0xFFFFFFFF;
static constexpr uint32_t SLIDER_SELECTED_OUTLINE_COLOR = 0xFFFFFF00;
static constexpr float SLIDER_SOURCE_BORDER_PIXELS      = 2.0f;

static void emitSliderQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1, float u0,
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

static void emitSliderNineSlice(BufferBuilder *builder, float x, float y, float width, float height,
                                float textureWidth, float textureHeight, float sourceBorderX,
                                float sourceBorderY, float destinationBorderX,
                                float destinationBorderY, uint32_t color)
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

    emitSliderQuad(builder, x0, y0, x1, y1, u0, v0, u1, v1, color);
    emitSliderQuad(builder, x1, y0, x2, y1, u1, v0, u2, v1, color);
    emitSliderQuad(builder, x2, y0, x3, y1, u2, v0, u3, v1, color);
    emitSliderQuad(builder, x0, y1, x1, y2, u0, v1, u1, v2, color);
    emitSliderQuad(builder, x1, y1, x2, y2, u1, v1, u2, v2, color);
    emitSliderQuad(builder, x2, y1, x3, y2, u2, v1, u3, v2, color);
    emitSliderQuad(builder, x0, y2, x1, y3, u0, v2, u1, v3, color);
    emitSliderQuad(builder, x1, y2, x2, y3, u1, v2, u2, v3, color);
    emitSliderQuad(builder, x2, y2, x3, y3, u2, v2, u3, v3, color);
}

UIControl_Slider::UIControl_Slider(float x, float y, float width, float height,
                                   const std::wstring &name, float minValue, float maxValue,
                                   float defaultValue, float increment)
    : UIControl("Control_Slider", x, y, width, height), m_name(name), m_text(name), m_value(0.0f),
      m_minValue(std::min(minValue, maxValue)), m_maxValue(std::max(minValue, maxValue)),
      m_defaultValue(defaultValue), m_increment(std::max(increment, 0.0f)), m_dragging(false),
      m_backgroundTexture("textures/ui/slider_background.png"),
      m_highlightTexture("textures/ui/slider_background_highlight.png"),
      m_graphicTexture("textures/ui/slider_graphic.png")
{
    if (std::fabs(m_maxValue - m_minValue) < 0.0001f)
    {
        m_maxValue = m_minValue + 1.0f;
    }

    m_defaultValue = quantizeValue(defaultValue);
    m_value        = m_defaultValue;
}

UIControl_Slider::~UIControl_Slider() {}

void UIControl_Slider::render()
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
    float roundedX      = std::round(rect.x);
    float roundedY      = std::round(rect.y);
    float roundedWidth  = std::round(rect.width);
    float roundedHeight = std::round(rect.height);
    float backgroundPixelHeight =
            std::max(1.0f, (float) std::max(m_backgroundTexture.getPixelHeight(), 1));
    float backgroundPixelScale = std::max(1.0f, std::round(roundedHeight / backgroundPixelHeight));
    float backgroundBorderSize = backgroundPixelScale * SLIDER_SOURCE_BORDER_PIXELS;
    float outlineThickness     = 3.0f;
    float graphicHeight        = roundedHeight;
    float graphicWidth =
            std::round(graphicHeight * ((float) std::max(m_graphicTexture.getPixelWidth(), 1) /
                                        (float) std::max(m_graphicTexture.getPixelHeight(), 1)));
    float trackWidth = std::max(0.0f, roundedWidth - graphicWidth);
    float graphicX   = std::round(roundedX + trackWidth * getNormalizedValue());
    float graphicY   = roundedY;

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(&m_backgroundTexture);
    builder->setAlphaCutoff(0.01f);
    emitSliderNineSlice(builder, roundedX, roundedY, roundedWidth, roundedHeight,
                        (float) std::max(m_backgroundTexture.getPixelWidth(), 1),
                        backgroundPixelHeight, SLIDER_SOURCE_BORDER_PIXELS,
                        SLIDER_SOURCE_BORDER_PIXELS, backgroundBorderSize, backgroundBorderSize,
                        0xFFFFFFFFu);
    builder->end();
    builder->unbindTexture();

    if (m_selected)
    {
        float highlightX      = roundedX + backgroundBorderSize;
        float highlightY      = roundedY + backgroundBorderSize;
        float highlightWidth  = std::max(0.0f, roundedWidth - backgroundBorderSize * 2.0f);
        float highlightHeight = std::max(0.0f, roundedHeight - backgroundBorderSize * 2.0f);

        builder->begin(GL_TRIANGLES);
        builder->bindTexture(&m_highlightTexture);
        builder->setAlphaCutoff(0.01f);
        emitSliderQuad(builder, highlightX, highlightY, highlightX + highlightWidth,
                       highlightY + highlightHeight, 0.0f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFFu);
        builder->end();
        builder->unbindTexture();

        builder->begin(GL_TRIANGLES);
        builder->unbindTexture();
        builder->setAlphaCutoff(-1.0f);
        float outlineX0 = roundedX - outlineThickness;
        float outlineY0 = roundedY - outlineThickness;
        float outlineX1 = roundedX + roundedWidth + outlineThickness;
        float outlineY1 = roundedY + roundedHeight + outlineThickness;

        emitSliderQuad(builder, outlineX0, outlineY0, outlineX1, roundedY, 0.0f, 0.0f, 0.0f, 0.0f,
                       SLIDER_SELECTED_OUTLINE_COLOR);
        emitSliderQuad(builder, outlineX0, roundedY + roundedHeight, outlineX1, outlineY1, 0.0f,
                       0.0f, 0.0f, 0.0f, SLIDER_SELECTED_OUTLINE_COLOR);
        emitSliderQuad(builder, outlineX0, roundedY, roundedX, roundedY + roundedHeight, 0.0f, 0.0f,
                       0.0f, 0.0f, SLIDER_SELECTED_OUTLINE_COLOR);
        emitSliderQuad(builder, roundedX + roundedWidth, roundedY, outlineX1,
                       roundedY + roundedHeight, 0.0f, 0.0f, 0.0f, 0.0f,
                       SLIDER_SELECTED_OUTLINE_COLOR);
        builder->end();
    }

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(&m_graphicTexture);
    builder->setAlphaCutoff(0.01f);
    emitSliderQuad(builder, graphicX, graphicY, graphicX + graphicWidth, graphicY + graphicHeight,
                   0.0f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFF);
    builder->end();
    builder->unbindTexture();
    builder->setAlphaCutoff(-1.0f);

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();

    if (!m_text.empty())
    {
        float fontScale = font->snapScale(uiScale * SLIDER_TEXT_SCALE);
        float textWidth = font->getWidth(m_text, fontScale);
        float textX     = std::round(rect.x + (rect.width - textWidth) * 0.5f);
        float textBaselineY =
                std::round(rect.y + (rect.height - font->getLineHeight(fontScale)) * 0.5f +
                           font->getAscent(fontScale));
        float shadowPixelOffset = std::max(1.0f, std::round(uiScale * 0.5f));
        uint32_t textColor = m_selected ? SLIDER_SELECTED_TEXT_COLOR : SLIDER_DEFAULT_TEXT_COLOR;

        font->setNearest(true);
        font->draw(m_text, textX + shadowPixelOffset, textBaselineY + shadowPixelOffset, fontScale,
                   0xFF000000);
        font->draw(m_text, textX, textBaselineY, fontScale, textColor);
    }
}

float UIControl_Slider::getPreferredWidth() const
{
    return std::max(m_width, SLIDER_DEFAULT_WIDTH);
}

float UIControl_Slider::getPreferredHeight() const
{
    return std::max(m_height, SLIDER_DEFAULT_HEIGHT);
}

bool UIControl_Slider::stepValue(int direction)
{
    if (direction == 0)
    {
        return false;
    }

    return setValue(m_value + m_increment * (float) direction);
}

bool UIControl_Slider::onPointerPressed(int actualX, int, int actualWidth, int actualHeight)
{
    m_dragging = true;
    return updateValueFromActualX(actualX, actualWidth, actualHeight);
}

bool UIControl_Slider::onPointerMoved(int actualX, int, int actualWidth, int actualHeight)
{
    if (!m_dragging)
    {
        return false;
    }

    return updateValueFromActualX(actualX, actualWidth, actualHeight);
}

bool UIControl_Slider::onPointerReleased(int actualX, int, int actualWidth, int actualHeight)
{
    bool changed = false;
    if (m_dragging)
    {
        changed = updateValueFromActualX(actualX, actualWidth, actualHeight);
    }

    m_dragging = false;
    return changed;
}

bool UIControl_Slider::setValue(float value)
{
    float clampedValue = quantizeValue(value);
    if (std::fabs(m_value - clampedValue) < 0.0001f)
    {
        return false;
    }

    m_value = clampedValue;
    return true;
}

float UIControl_Slider::getValue() const { return m_value; }

float UIControl_Slider::getMinimumValue() const { return m_minValue; }

float UIControl_Slider::getMaximumValue() const { return m_maxValue; }

float UIControl_Slider::getDefaultValue() const { return m_defaultValue; }

float UIControl_Slider::getIncrement() const { return m_increment; }

const std::wstring &UIControl_Slider::getName() const { return m_name; }

void UIControl_Slider::resetToDefaultValue() { setValue(m_defaultValue); }

void UIControl_Slider::setText(const std::wstring &text) { m_text = text; }

const std::wstring &UIControl_Slider::getText() const { return m_text; }

float UIControl_Slider::getNormalizedValue() const
{
    float range = m_maxValue - m_minValue;
    if (range <= 0.0001f)
    {
        return 0.0f;
    }

    return Mth::clampf((m_value - m_minValue) / range, 0.0f, 1.0f);
}

float UIControl_Slider::quantizeValue(float value) const
{
    float clampedValue = Mth::clampf(value, m_minValue, m_maxValue);
    if (m_increment <= 0.0001f)
    {
        return clampedValue;
    }

    float steps = std::round((clampedValue - m_minValue) / m_increment);
    return Mth::clampf(m_minValue + steps * m_increment, m_minValue, m_maxValue);
}

bool UIControl_Slider::updateValueFromActualX(int actualX, int actualWidth, int actualHeight)
{
    UIScreen::Rect rect    = getActualRect(actualWidth, actualHeight);
    float graphicAspect    = 1.0f;
    int graphicPixelHeight = m_graphicTexture.getPixelHeight();
    int graphicPixelWidth  = m_graphicTexture.getPixelWidth();
    if (graphicPixelWidth > 0 && graphicPixelHeight > 0)
    {
        graphicAspect = (float) graphicPixelWidth / (float) graphicPixelHeight;
    }

    float graphicWidth    = rect.height * graphicAspect;
    float travelWidth     = std::max(1.0f, rect.width - graphicWidth);
    float centeredX       = (float) actualX - rect.x - graphicWidth * 0.5f;
    float normalizedValue = Mth::clampf(centeredX / travelWidth, 0.0f, 1.0f);
    return setValue(m_minValue + normalizedValue * (m_maxValue - m_minValue));
}
