#pragma once

#include <string>
#include <vector>

#include "UIComponent.h"

class UILayer {
public:
    explicit UILayer(const std::string &name);
    ~UILayer();

    void tick();
    void render();

    void addComponent(UIComponent *component);
    void removeComponent(UIComponent *component);

    const std::string &getName() const;

private:
    std::string m_name;
    std::vector<UIComponent *> m_components;
};
