#include "UIComponent.h"

UIComponent::UIComponent(const std::string &name) : m_name(name), m_visible(true) {}

UIComponent::~UIComponent() {}

void UIComponent::tick() {}

void UIComponent::render() {}

void UIComponent::setVisible(bool visible) { m_visible = visible; }

void UIComponent::toggleVisible() { m_visible = !m_visible; }

bool UIComponent::isVisible() const { return m_visible; }

const std::string &UIComponent::getName() const { return m_name; }
