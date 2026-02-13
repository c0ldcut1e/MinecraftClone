#include "MouseButtonPressedEvent.h"

MouseButtonPressedEvent::MouseButtonPressedEvent(int button) : m_button(button) {}

int MouseButtonPressedEvent::getButton() const { return m_button; }
