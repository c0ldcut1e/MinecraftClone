#include "UIPanelProperties_TestMenu.h"

UIPanelProperties_TestMenu::UIPanelProperties_TestMenu()
    : UIPanelProperties(32.0f, 32.0f, 150.0f, 150.0f, 1.0f)
{}

UIPanelProperties_TestMenu::UIPanelProperties_TestMenu(float x, float y, float width, float height,
                                                       float transparency, void *customProperties)
    : UIPanelProperties(x, y, width, height, transparency, customProperties)
{}

UIPanelProperties_TestMenu::~UIPanelProperties_TestMenu() {}

const char *UIPanelProperties_TestMenu::getTexturePath() const
{
    return "textures/ui/panel9grid.png";
}
