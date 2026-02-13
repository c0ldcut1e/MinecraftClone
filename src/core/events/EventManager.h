#pragma once

#include <functional>
#include <vector>

#include "Event.h"

class EventManager {
public:
    using Listener = std::function<void(Event &)>;

    static void push(Event *event);
    static void addListener(const Listener &listener);
    static void process();

private:
    static std::vector<Event *> s_events;
    static std::vector<Listener> s_listeners;
};
