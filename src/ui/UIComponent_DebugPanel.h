#pragma once

#include "UIComponent.h"

class UIComponent_DebugPanel : public UIComponent {
public:
    UIComponent_DebugPanel();
    ~UIComponent_DebugPanel() override;

    void tick() override;
    void render() override;
};
