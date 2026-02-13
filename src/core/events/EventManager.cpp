#include "EventManager.h"

std::vector<Event *> EventManager::s_events;
std::vector<EventManager::Listener> EventManager::s_listeners;

void EventManager::push(Event *event) { s_events.push_back(event); }

void EventManager::addListener(const Listener &listener) { s_listeners.push_back(listener); }

void EventManager::process() {
    for (Event *event : s_events) {
        for (auto &listener : s_listeners) listener(*event);
        delete event;
    }

    s_events.clear();
}
