#pragma once

#include "Event.h"

class KeyReleasedEvent : public Event {
public:
    explicit KeyReleasedEvent(int key);

    int getKey() const;

private:
    int m_key;
};
