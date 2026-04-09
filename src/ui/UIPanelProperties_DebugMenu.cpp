#include "UIPanelProperties_DebugMenu.h"

UIPanelProperties_DebugMenu::UIPanelProperties_DebugMenu()
    : UIPanelProperties(32.0f, 32.0f, 500.0f, 500.0f, 1.0f)
{}

UIPanelProperties_DebugMenu::UIPanelProperties_DebugMenu(float x, float y, float width,
                                                         float height, float transparency,
                                                         void *customProperties)
    : UIPanelProperties(x, y, width, height, transparency, customProperties)
{}

UIPanelProperties_DebugMenu::~UIPanelProperties_DebugMenu() {}

const char *UIPanelProperties_DebugMenu::getTexturePath() const { return "textures/ui/panel.png"; }
