#pragma once

#include "Event.h"

class EventDispatcher {
public:
    explicit EventDispatcher(Event &event) : m_event(event) {}

    template<typename T>
    T *as() {
        return dynamic_cast<T *>(&m_event);
    }

    template<typename T, typename F>
    bool dispatch(const F &func) {
        T *event = dynamic_cast<T *>(&m_event);
        if (event) {
            func(*event);
            return true;
        }

        return false;
    }

private:
    Event &m_event;
};
