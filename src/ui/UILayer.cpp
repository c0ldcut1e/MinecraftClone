#include "UILayer.h"

#include <algorithm>

UILayer::UILayer(const std::string &name) : m_name(name) {}

UILayer::~UILayer() {}

void UILayer::tick() {
    for (UIComponent *component : m_components)
        if (component) component->tick();
}

void UILayer::render() {
    for (UIComponent *component : m_components)
        if (component) component->render();
}

void UILayer::addComponent(UIComponent *component) {
    if (!component) return;
    m_components.push_back(component);
}

void UILayer::removeComponent(UIComponent *component) {
    if (!component) return;
    m_components.erase(std::remove(m_components.begin(), m_components.end(), component), m_components.end());
}

const std::string &UILayer::getName() const { return m_name; }
