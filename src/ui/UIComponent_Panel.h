#pragma once

#include <memory>

#include "../rendering/Texture.h"
#include "UIComponent.h"

class UIPanelProperties;

class UIComponent_Panel : public UIComponent
{
public:
    explicit UIComponent_Panel(std::unique_ptr<UIPanelProperties> properties);
    ~UIComponent_Panel() override;

    void render() override;

    UIPanelProperties *getProperties();
    const UIPanelProperties *getProperties() const;

private:
    std::unique_ptr<UIPanelProperties> m_properties;
    Texture m_texture;
};
