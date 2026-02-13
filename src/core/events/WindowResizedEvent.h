#pragma once

#include "Event.h"

class WindowResizedEvent : public Event {
public:
    WindowResizedEvent(int width, int height);

    int getWidth() const;
    int getHeight() const;

private:
    int m_width;
    int m_height;
};
