#include "UIComponent.h"

UIComponent::UIComponent(const std::string &name) : m_name(name) {}

UIComponent::~UIComponent() {}

void UIComponent::tick() {}

void UIComponent::render() {}

const std::string &UIComponent::getName() const { return m_name; }
