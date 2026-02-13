#include "KeyPressedEvent.h"

KeyPressedEvent::KeyPressedEvent(int key) : m_key(key) {}

int KeyPressedEvent::getKey() const { return m_key; }
