#pragma once

#include <string>

#include "../rendering/Texture.h"
#include "UIControl.h"

class UIControl_Checkbox : public UIControl
{
public:
    UIControl_Checkbox(float x, float y, float width, float height, const std::wstring &label,
                       bool checked = false);
    ~UIControl_Checkbox() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;

    void activate() override;
    bool onPointerPressed(int actualX, int actualY, int actualWidth, int actualHeight) override;

    void setChecked(bool checked);
    bool isChecked() const;

private:
    float measureLabelWidth() const;

    std::wstring m_label;
    bool m_checked;
    Texture m_backgroundTexture;
    Texture m_overTexture;
    Texture m_tickTexture;
};
