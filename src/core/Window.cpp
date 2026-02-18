#include "Window.h"

#include <stdexcept>

Window::Window(int width, int height, const char *title) : m_window(nullptr) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) throw std::runtime_error("glfwCreateWindow failed");

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() const { glfwPollEvents(); }

void Window::swapBuffers() const { glfwSwapBuffers(m_window); }

GLFWwindow *Window::getHandle() const { return m_window; }
