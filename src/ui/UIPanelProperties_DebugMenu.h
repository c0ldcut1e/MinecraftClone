#pragma once

#include "UIPanelProperties.h"

class UIPanelProperties_DebugMenu : public UIPanelProperties
{
public:
    UIPanelProperties_DebugMenu();
    UIPanelProperties_DebugMenu(float x, float y, float width, float height,
                                float transparency = 1.0f, void *customProperties = nullptr);
    ~UIPanelProperties_DebugMenu() override;

    const char *getTexturePath() const override;
};
