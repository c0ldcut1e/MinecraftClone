#include "UIScene_MainMenu.h"

#include <cmath>
#include <functional>
#include <string>

#define MAIN_MENU_LABEL_ID      1
#define MAIN_MENU_CHECKBOX_1_ID 100
#define MAIN_MENU_CHECKBOX_2_ID 101
#define MAIN_MENU_SLIDER_ID     200

UIScene_MainMenu::UIScene_MainMenu()
    : UIScene("Scene_MainMenu"), m_layerRoot(new UILayer("Layer_MainMenu")), m_multiList(nullptr),
      m_testSlider(nullptr)
{
    m_multiList = new UIControl_MultiList(710.0f, 165.0f, 500.0f, 750.0f);
    m_multiList->setWrapSelection(true);
    m_multiList->setContentPadding(18.0f, 24.0f);
    m_multiList->setRowSpacing(8.0f);
    m_multiList->setCheckboxRowHeight(28.0f);
    m_multiList->setSliderRowHeight(36.0f);
    m_multiList->setLabelRowHeight(20.0f);
    m_multiList->setCheckboxUpdatedCallback(std::bind(&UIScene_MainMenu::handleCheckboxUpdated,
                                                      this, std::placeholders::_1,
                                                      std::placeholders::_2));
    m_multiList->setSliderUpdatedCallback(std::bind(&UIScene_MainMenu::handleSliderUpdated, this,
                                                    std::placeholders::_1, std::placeholders::_2));
    m_multiList->addNewLabel(MAIN_MENU_LABEL_ID, L"Test");
    m_multiList->addNewCheckbox(MAIN_MENU_CHECKBOX_1_ID, L"Render Clouds", false);
    m_multiList->addNewCheckbox(MAIN_MENU_CHECKBOX_2_ID, L"Custom Skin Animation", true);
    m_testSlider =
            m_multiList->addNewSlider(MAIN_MENU_SLIDER_ID, L"Gamma", 0.0f, 100.0f, 50.0f, 1.0f);
    m_testSlider->setText(L"Gamma: 100");
    m_layerRoot->addComponent(m_multiList);
    addLayer(m_layerRoot);
}

UIScene_MainMenu::~UIScene_MainMenu() {}

void UIScene_MainMenu::handleCheckboxUpdated(uint32_t, bool) {}

void UIScene_MainMenu::handleSliderUpdated(uint32_t id, float value)
{
    if (id != MAIN_MENU_SLIDER_ID || !m_testSlider)
    {
        return;
    }

    int displayValue = (int) std::lround(value);
    m_testSlider->setText(m_testSlider->getName() + L" " + std::to_wstring(displayValue));
}
