#include "GlfwEventBridge.h"

#include <GLFW/glfw3.h>

#include "EventManager.h"
#include "KeyPressedEvent.h"
#include "KeyReleasedEvent.h"
#include "MouseButtonPressedEvent.h"
#include "MouseMovedEvent.h"
#include "WindowResizedEvent.h"

static double s_lastX = 0.0;
static double s_lastY = 0.0;
static bool s_first   = true;

static void keyCallback(GLFWwindow *, int key, int, int action, int) {
    if (action == GLFW_PRESS) EventManager::push(new KeyPressedEvent(key));
    else if (action == GLFW_RELEASE)
        EventManager::push(new KeyReleasedEvent(key));
}

static void cursorCallback(GLFWwindow *, double x, double y) {
    if (s_first) {
        s_lastX = x;
        s_lastY = y;
        s_first = false;
        return;
    }

    EventManager::push(new MouseMovedEvent(x - s_lastX, s_lastY - y));

    s_lastX = x;
    s_lastY = y;
}

static void mouseButtonCallback(GLFWwindow *, int button, int action, int) {
    if (action == GLFW_PRESS) EventManager::push(new MouseButtonPressedEvent(button));
}

static void framebufferSizeCallback(GLFWwindow *, int width, int height) {
    if (width <= 0 || height <= 0) return;
    EventManager::push(new WindowResizedEvent(width, height));
}

void initGlfwEventBridge(void *windowHandle) {
    GLFWwindow *window = (GLFWwindow *) windowHandle;

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

void resetGlfwMouseState() { s_first = true; }
