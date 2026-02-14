#pragma once

#include "UIScene.h"

class UIScene_DebugOverlay : public UIScene {
public:
    UIScene_DebugOverlay();
    ~UIScene_DebugOverlay() override;

private:
    UILayer *m_layerRoot;
    UIComponent *m_debugPanel;
};
