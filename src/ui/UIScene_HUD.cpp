#include "UIScene_HUD.h"

#include "UIComponent_Crosshair.h"

UIScene_HUD::UIScene_HUD() : UIScene("SceneHUD"), m_layerRoot(new UILayer("LayerHUD")), m_crosshair(new UIComponent_Crosshair()) {
    m_layerRoot->addComponent(m_crosshair);
    addLayer(m_layerRoot);
}

UIScene_HUD::~UIScene_HUD() {}
