#include "MouseMovedEvent.h"

MouseMovedEvent::MouseMovedEvent(double deltaX, double deltaY) : m_deltaX(deltaX), m_deltaY(deltaY) {}

double MouseMovedEvent::getDeltaX() const { return m_deltaX; }

double MouseMovedEvent::getDeltaY() const { return m_deltaY; }
