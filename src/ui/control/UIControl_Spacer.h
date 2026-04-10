#pragma once

#include "UIControl.h"

class UIControl_Spacer : public UIControl
{
public:
    UIControl_Spacer(float x, float y, float width, float height);
    ~UIControl_Spacer() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;
    bool isSelectable() const override;
};
