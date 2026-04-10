#include "UIScene_MainMenu.h"

#include <cmath>
#include <functional>
#include <string>

#include "../UIScreen.h"

UIScene_MainMenu::UIScene_MainMenu()
    : UIScene("Scene_MainMenu"), m_layerRoot(new UILayer("Layer_MainMenu")), m_multiList(nullptr),
      m_testSlider(nullptr)
{
    m_multiList = new UIControl_MultiList(0.0f, 0.0f, 0.0f, 0.0f);
    m_multiList->addNewLabel(1, L"Label");
    m_multiList->addNewButton(100, L"Button");
    m_multiList->addNewCheckbox(200, L"Checkbox");
    m_multiList->addNewSlider(300, L"Slider", 1.0f, 10.0f, 5.0f, 1.0f);
    m_multiList->resizeHorizontal();
    m_multiList->resizeVertical();
    m_multiList->addNewSpacer(350, 72.0f);
    m_multiList->addNewProgressBar(400, 0.45f);
    m_multiList->setPosition((UIScreen::WIDTH - m_multiList->getWidth()) * 0.5f,
                             (UIScreen::HEIGHT - m_multiList->getHeight()) * 0.5f);
    m_layerRoot->addComponent(m_multiList);
    addLayer(m_layerRoot);
}

UIScene_MainMenu::~UIScene_MainMenu() {}
