#pragma once

#include <cstdint>
#include <string>

#include "../../rendering/Texture.h"
#include "UIControl.h"

class UIControl_Button : public UIControl
{
public:
    UIControl_Button(float x, float y, float width, float height, const std::wstring &label);
    ~UIControl_Button() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;

    void activate() override;
    bool onPointerPressed(int actualX, int actualY, int actualWidth, int actualHeight) override;

    const std::wstring &getLabel() const;
    void setLabel(const std::wstring &label);

protected:
    UIControl_Button(float x, float y, float width, float height, const std::wstring &label,
                     const char *normalTexturePath, const char *highlightTexturePath,
                     float preferredHeight, float preferredWidth = 0.0f);

private:
    std::wstring m_label;
    Texture m_normalTexture;
    Texture m_highlightTexture;
    float m_preferredHeight;
    float m_preferredWidth;
    bool m_wasPressed;
    uint32_t m_pressedAtMs;
};
