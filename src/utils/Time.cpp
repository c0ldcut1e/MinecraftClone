#include "Time.h"

#include <GLFW/glfw3.h>

double Time::s_delta = 0.0;
double Time::s_last  = 0.0;

void Time::update() {
    double now = glfwGetTime();
    s_delta    = now - s_last;
    s_last     = now;
}

double Time::delta() { return s_delta; }
