#include "Time.h"

#include <GLFW/glfw3.h>

double Time::s_delta     = 0.0;
double Time::s_last      = 0.0;
double Time::s_tickDelta = 0.05;

void Time::update() {
    double now = glfwGetTime();
    s_delta    = now - s_last;
    s_last     = now;
}

double Time::getDelta() { return s_delta; }

double Time::getTickDelta() { return s_tickDelta; }

void Time::setTickDelta(double deltaTime) { s_tickDelta = deltaTime; }
