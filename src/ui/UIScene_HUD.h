#pragma once

#include "UIScene.h"

class UIScene_HUD : public UIScene {
public:
    UIScene_HUD();
    ~UIScene_HUD() override;

private:
    UILayer *m_layerRoot;
    UIComponent *m_crosshair;
};
