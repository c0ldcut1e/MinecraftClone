#pragma once

#include "Event.h"

class KeyPressedEvent : public Event {
public:
    explicit KeyPressedEvent(int key);

    int getKey() const;

private:
    int m_key;
};
