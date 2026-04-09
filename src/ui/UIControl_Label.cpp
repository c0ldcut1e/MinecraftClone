#include "UIControl_Label.h"

#include <algorithm>
#include <cmath>

#include "../core/Minecraft.h"
#include "../rendering/Font.h"
#include "UIScreen.h"

static constexpr float LABEL_TEXT_SCALE = 0.85f;

UIControl_Label::UIControl_Label(float x, float y, float width, float height,
                                 const std::wstring &text, uint32_t color, bool shadow)
    : UIControl("Control_Label", x, y, width, height), m_text(text), m_color(color),
      m_shadow(shadow)
{}

UIControl_Label::~UIControl_Label() {}

void UIControl_Label::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    Font *font = minecraft->getDefaultFont();
    if (!font || m_text.empty())
    {
        return;
    }

    int actualWidth     = minecraft->getWidth();
    int actualHeight    = minecraft->getHeight();
    UIScreen::Rect rect = getActualRect(actualWidth, actualHeight);
    float uiScale       = UIScreen::scaleUniform(actualWidth, actualHeight);
    float fontScale     = font->snapScale(uiScale * LABEL_TEXT_SCALE);
    float textX         = std::round(rect.x);
    float textBaselineY =
            std::round(rect.y + (rect.height - font->getLineHeight(fontScale)) * 0.5f +
                       font->getAscent(fontScale));
    float shadowPixelOffset = std::max(1.0f, std::round(uiScale * 0.5f));

    font->setNearest(true);
    if (m_shadow)
    {
        font->draw(m_text, textX + shadowPixelOffset, textBaselineY + shadowPixelOffset, fontScale,
                   0xFF484848);
    }
    font->draw(m_text, textX, textBaselineY, fontScale, m_color);
}

float UIControl_Label::getPreferredWidth() const { return measureTextWidth(); }

float UIControl_Label::getPreferredHeight() const { return 20.0f; }

bool UIControl_Label::isSelectable() const { return false; }

void UIControl_Label::setText(const std::wstring &text) { m_text = text; }

const std::wstring &UIControl_Label::getText() const { return m_text; }

void UIControl_Label::setColor(uint32_t color) { m_color = color; }

void UIControl_Label::setShadow(bool shadow) { m_shadow = shadow; }

float UIControl_Label::measureTextWidth() const
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return m_width;
    }

    Font *font = minecraft->getDefaultFont();
    if (!font)
    {
        return m_width;
    }

    return font->getWidth(m_text, LABEL_TEXT_SCALE);
}
