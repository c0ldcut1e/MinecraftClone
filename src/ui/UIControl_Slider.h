#pragma once

#include <string>

#include "../rendering/Texture.h"
#include "UIControl.h"

class UIControl_Slider : public UIControl
{
public:
    UIControl_Slider(float x, float y, float width, float height, const std::wstring &name,
                     float minValue, float maxValue, float defaultValue, float increment);
    ~UIControl_Slider() override;

    void render() override;

    float getPreferredWidth() const override;
    float getPreferredHeight() const override;

    bool stepValue(int direction) override;
    bool onPointerPressed(int actualX, int actualY, int actualWidth, int actualHeight) override;
    bool onPointerMoved(int actualX, int actualY, int actualWidth, int actualHeight) override;
    bool onPointerReleased(int actualX, int actualY, int actualWidth, int actualHeight) override;

    bool setValue(float value);
    float getValue() const;
    float getMinimumValue() const;
    float getMaximumValue() const;
    float getDefaultValue() const;
    float getIncrement() const;
    const std::wstring &getName() const;
    void resetToDefaultValue();
    void setText(const std::wstring &text);
    const std::wstring &getText() const;

private:
    float getNormalizedValue() const;
    float quantizeValue(float value) const;
    bool updateValueFromActualX(int actualX, int actualWidth, int actualHeight);

    std::wstring m_name;
    std::wstring m_text;
    float m_value;
    float m_minValue;
    float m_maxValue;
    float m_defaultValue;
    float m_increment;
    bool m_dragging;
    Texture m_backgroundTexture;
    Texture m_highlightTexture;
    Texture m_graphicTexture;
};
