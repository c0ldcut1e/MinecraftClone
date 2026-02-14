#include "UIScene_DebugOverlay.h"

#include "UIComponent_DebugPanel.h"

UIScene_DebugOverlay::UIScene_DebugOverlay() : UIScene("SceneDebugOverlay"), m_layerRoot(new UILayer("LayerDebugOverlay")), m_debugPanel(new UIComponent_DebugPanel()) {
    m_layerRoot->addComponent(m_debugPanel);
    addLayer(m_layerRoot);
}

UIScene_DebugOverlay::~UIScene_DebugOverlay() {}
