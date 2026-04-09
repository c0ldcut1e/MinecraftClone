#pragma once

#include <string>

#include "UIControl.h"

class UIControl_Label : public UIControl
{
public:
    UIControl_Label(float x, float y, float width, float height, const std::wstring &text,
                    uint32_t color = 0xFF717171u, bool shadow = false);
    ~UIControl_Label() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;
    bool isSelectable() const override;

    void setText(const std::wstring &text);
    const std::wstring &getText() const;
    void setColor(uint32_t color);
    void setShadow(bool shadow);

private:
    float measureTextWidth() const;

    std::wstring m_text;
    uint32_t m_color;
    bool m_shadow;
};
