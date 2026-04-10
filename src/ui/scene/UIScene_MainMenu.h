#pragma once

#include "../control/UIControl_MultiList.h"
#include "../control/UIControl_Slider.h"
#include "UIScene.h"

class UIScene_MainMenu : public UIScene
{
public:
    UIScene_MainMenu();
    ~UIScene_MainMenu() override;

private:
    UILayer *m_layerRoot;
    UIControl_MultiList *m_multiList;
    UIControl_Slider *m_testSlider;
};
