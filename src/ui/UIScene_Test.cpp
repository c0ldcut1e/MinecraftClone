#include "UIScene_Test.h"

#include <memory>

#include "UIComponent_Panel.h"
#include "UIPanelProperties_TestMenu.h"

UIScene_Test::UIScene_Test()
    : UIScene("Scene_Test"), m_layerRoot(new UILayer("Layer_Test")),
      m_panel(new UIComponent_Panel(std::make_unique<UIPanelProperties_TestMenu>()))
{
    m_layerRoot->addComponent(m_panel);
    addLayer(m_layerRoot);
}

UIScene_Test::~UIScene_Test() {}
