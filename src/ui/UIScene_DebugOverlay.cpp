#include "UIScene_DebugOverlay.h"

#include "UIComponent_DebugMenu.h"

UIScene_DebugOverlay::UIScene_DebugOverlay()
    : UIScene("Scene_DebugOverlay"), m_layerRoot(new UILayer("Layer_DebugOverlay")),
      m_debugMenu(new UIComponent_DebugMenu())
{
    m_layerRoot->addComponent(m_debugMenu);
    addLayer(m_layerRoot);
}

UIScene_DebugOverlay::~UIScene_DebugOverlay() {}
