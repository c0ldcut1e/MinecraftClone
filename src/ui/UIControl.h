#pragma once

#include <cstdint>

#include "UIComponent.h"
#include "UIScreen.h"

class UIControl : public UIComponent
{
public:
    UIControl(const std::string &name, float x, float y, float width, float height);
    ~UIControl() override;

    void setPosition(float x, float y);
    void setSize(float width, float height);
    void setBounds(float x, float y, float width, float height);
    void setId(uint32_t id);
    void setLayoutOrigin(float x, float y);
    void clearLayoutOrigin();

    uint32_t getId() const;
    float getX() const;
    float getY() const;
    float getWidth() const;
    float getHeight() const;
    UIScreen::Rect getActualRect(int actualWidth, int actualHeight) const;

    virtual float getPreferredWidth() const;
    virtual float getPreferredHeight() const;

    virtual bool isSelectable() const;
    virtual void setSelected(bool selected);
    bool isSelected() const;

    virtual void activate();
    virtual bool stepValue(int direction);
    virtual bool onPointerPressed(int actualX, int actualY, int actualWidth, int actualHeight);
    virtual bool onPointerMoved(int actualX, int actualY, int actualWidth, int actualHeight);
    virtual bool onPointerReleased(int actualX, int actualY, int actualWidth, int actualHeight);

    bool containsActualPoint(int actualX, int actualY, int actualWidth, int actualHeight) const;

protected:
    uint32_t m_id;
    float m_x;
    float m_y;
    float m_width;
    float m_height;
    bool m_selected;
    bool m_hasLayoutOrigin;
    float m_layoutOriginX;
    float m_layoutOriginY;
};
