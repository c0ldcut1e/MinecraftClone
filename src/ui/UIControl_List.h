#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "UIComponent_Panel.h"
#include "UIControl.h"

class UIControl_List : public UIControl
{
public:
    UIControl_List(const std::string &name, float x, float y, float width, float height,
                   float transparency = 1.0f);
    ~UIControl_List() override;

    void tick() override;
    void render() override;

    void addControl(UIControl *control);
    void removeControl(UIControl *control);
    UIControl *getControlById(uint32_t id) const;

    void resizeVertical(float padding = 24.0f);
    void resizeHorizontal(float padding = 24.0f);

protected:
    void syncPanel();

    std::vector<UIControl *> m_controls;
    std::unique_ptr<UIComponent_Panel> m_panel;
    float m_transparency;
    float m_contentPaddingX;
    float m_contentPaddingY;
};
