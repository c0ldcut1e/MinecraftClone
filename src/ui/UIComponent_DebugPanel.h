#pragma once

#include "UIComponent.h"

#include <string>
#include <vector>

class UIComponent_DebugPanel : public UIComponent
{
public:
    UIComponent_DebugPanel();
    ~UIComponent_DebugPanel() override;

    void tick() override;
    void render() override;

private:
    void refreshLines();

    std::vector<std::wstring> m_cachedLines;
    double m_statsRefreshTimer;
    double m_statsRefreshInterval;
};
