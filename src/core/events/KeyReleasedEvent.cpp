#include "KeyReleasedEvent.h"

KeyReleasedEvent::KeyReleasedEvent(int key) : m_key(key) {}

int KeyReleasedEvent::getKey() const { return m_key; }
