#pragma once

#include "UIControl_MultiList.h"
#include "UIControl_Slider.h"
#include "UIScene.h"

class UIScene_MainMenu : public UIScene
{
public:
    UIScene_MainMenu();
    ~UIScene_MainMenu() override;

private:
    void handleCheckboxUpdated(uint32_t id, bool checked);
    void handleSliderUpdated(uint32_t id, float value);

    UILayer *m_layerRoot;
    UIControl_MultiList *m_multiList;
    UIControl_Slider *m_testSlider;
};
