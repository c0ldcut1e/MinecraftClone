#include "UIScene_Test.h"

#include <memory>

#include "../UIPanelProperties_TestMenu.h"
#include "../component/UIComponent_Panel.h"

UIScene_Test::UIScene_Test()
    : UIScene("Scene_Test"), m_layerRoot(new UILayer("Layer_Test")),
      m_panel(new UIComponent_Panel(std::make_unique<UIPanelProperties_TestMenu>()))
{
    m_layerRoot->addComponent(m_panel);
    addLayer(m_layerRoot);
}

UIScene_Test::~UIScene_Test() {}
