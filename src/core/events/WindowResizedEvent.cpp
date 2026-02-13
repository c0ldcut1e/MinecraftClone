#include "WindowResizedEvent.h"

WindowResizedEvent::WindowResizedEvent(int width, int height) : m_width(width), m_height(height) {}

int WindowResizedEvent::getWidth() const { return m_width; }

int WindowResizedEvent::getHeight() const { return m_height; }
