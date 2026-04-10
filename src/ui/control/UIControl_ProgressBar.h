#pragma once

#include "../../rendering/Texture.h"
#include "UIControl.h"

class UIControl_ProgressBar : public UIControl
{
public:
    UIControl_ProgressBar(float x, float y, float width, float height, float progress = 0.0f);
    ~UIControl_ProgressBar() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;
    bool isSelectable() const override;

    bool setProgress(float progress);
    float getProgress() const;

private:
    float m_progress;
    Texture m_backgroundTexture;
    Texture m_fillerTexture;
};
