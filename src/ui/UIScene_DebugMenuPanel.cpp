#include "UIScene_DebugMenuPanel.h"

#include <memory>

#include "UIComponent_Panel.h"
#include "UIPanelProperties_DebugMenu.h"

UIScene_DebugMenuPanel::UIScene_DebugMenuPanel()
    : UIScene("SceneDebugMenuPanel"), m_layerRoot(new UILayer("LayerDebugMenuPanel")),
      m_panel(new UIComponent_Panel(std::make_unique<UIPanelProperties_DebugMenu>()))
{
    m_layerRoot->addComponent(m_panel);
    addLayer(m_layerRoot);
}

UIScene_DebugMenuPanel::~UIScene_DebugMenuPanel() {}
