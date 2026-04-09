#pragma once

#include "UIPanelProperties.h"

class UIPanelProperties_TestMenu : public UIPanelProperties
{
public:
    UIPanelProperties_TestMenu();
    UIPanelProperties_TestMenu(float x, float y, float width, float height,
                               float transparency = 1.0f, void *customProperties = nullptr);
    ~UIPanelProperties_TestMenu() override;

    const char *getTexturePath() const override;
};
