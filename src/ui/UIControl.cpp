#include "UIControl.h"

#include "UIScreen.h"

UIControl::UIControl(const std::string &name, float x, float y, float width, float height)
    : UIComponent(name), m_id(0), m_x(x), m_y(y), m_width(width), m_height(height),
      m_selected(false), m_hasLayoutOrigin(false), m_layoutOriginX(0.0f), m_layoutOriginY(0.0f)
{}

UIControl::~UIControl() {}

void UIControl::setPosition(float x, float y)
{
    m_x = x;
    m_y = y;
}

void UIControl::setSize(float width, float height)
{
    m_width  = width;
    m_height = height;
}

void UIControl::setBounds(float x, float y, float width, float height)
{
    m_x      = x;
    m_y      = y;
    m_width  = width;
    m_height = height;
}

void UIControl::setId(uint32_t id) { m_id = id; }

void UIControl::setLayoutOrigin(float x, float y)
{
    m_hasLayoutOrigin = true;
    m_layoutOriginX   = x;
    m_layoutOriginY   = y;
}

void UIControl::clearLayoutOrigin() { m_hasLayoutOrigin = false; }

uint32_t UIControl::getId() const { return m_id; }

float UIControl::getX() const { return m_x; }

float UIControl::getY() const { return m_y; }

float UIControl::getWidth() const { return m_width; }

float UIControl::getHeight() const { return m_height; }

UIScreen::Rect UIControl::getActualRect(int actualWidth, int actualHeight) const
{
    float x = UIScreen::toActualX(m_x, actualWidth);
    float y = UIScreen::toActualY(m_y, actualWidth, actualHeight);
    if (m_hasLayoutOrigin)
    {
        x = UIScreen::toActualOffsetX(m_layoutOriginX, m_x, actualWidth, actualHeight);
        y = UIScreen::toActualOffsetY(m_layoutOriginY, m_y, actualWidth, actualHeight);
    }

    return {x, y, UIScreen::toActualLength(m_width, actualWidth, actualHeight),
            UIScreen::toActualLength(m_height, actualWidth, actualHeight)};
}

float UIControl::getPreferredWidth() const { return m_width; }

float UIControl::getPreferredHeight() const { return m_height; }

bool UIControl::isSelectable() const { return true; }

void UIControl::setSelected(bool selected) { m_selected = selected; }

bool UIControl::isSelected() const { return m_selected; }

void UIControl::activate() {}

bool UIControl::stepValue(int) { return false; }

bool UIControl::onPointerPressed(int, int, int, int) { return false; }

bool UIControl::onPointerMoved(int, int, int, int) { return false; }

bool UIControl::onPointerReleased(int, int, int, int) { return false; }

bool UIControl::containsActualPoint(int actualX, int actualY, int actualWidth,
                                    int actualHeight) const
{
    UIScreen::Rect rect = getActualRect(actualWidth, actualHeight);
    return (float) actualX >= rect.x && (float) actualX <= rect.x + rect.width &&
           (float) actualY >= rect.y && (float) actualY <= rect.y + rect.height;
}
