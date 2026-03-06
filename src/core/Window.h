#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class Window
{
public:
    Window(int width, int height, const char *title);
    ~Window();

    bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;

    void enableVSync() const;
    void disableVSync() const;

    GLFWwindow *getHandle() const;

private:
    GLFWwindow *m_window;
};
