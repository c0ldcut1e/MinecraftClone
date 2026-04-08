#include "Lighting.h"

#include <atomic>

std::atomic<bool> g_lightingOn{true};

void Lighting::turnOn() { g_lightingOn.store(true); }

void Lighting::turnOff() { g_lightingOn.store(false); }

bool Lighting::isOn() { return g_lightingOn.load(); }
