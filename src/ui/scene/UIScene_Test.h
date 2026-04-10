#pragma once

#include "../component/UIComponent_Panel.h"
#include "UIScene.h"

class UIScene_Test : public UIScene
{
public:
    UIScene_Test();
    ~UIScene_Test() override;

private:
    UILayer *m_layerRoot;
    UIComponent_Panel *m_panel;
};
