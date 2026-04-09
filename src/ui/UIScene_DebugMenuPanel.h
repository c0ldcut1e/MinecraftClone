#pragma once

#include "UIScene.h"

class UIComponent_Panel;

class UIScene_DebugMenuPanel : public UIScene
{
public:
    UIScene_DebugMenuPanel();
    ~UIScene_DebugMenuPanel() override;

private:
    UILayer *m_layerRoot;
    UIComponent_Panel *m_panel;
};
