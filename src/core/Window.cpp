#include "Window.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include <glad/glad.h>

#ifndef _WIN32
static bool envTruthy(const char *name) {
    const char *variable = std::getenv(name);
    if (!variable || !*variable) return false;
    if (variable[0] == '0') return false;
    if (variable[0] == 'f' || variable[0] == 'F') return false;
    if (variable[0] == 'n' || variable[0] == 'N') return false;
    return true;
}

static bool isWaylandSession() {
    if (std::getenv("WAYLAND_DISPLAY")) return true;
    const char *type = std::getenv("XDG_SESSION_TYPE");
    if (!type) return false;
    return std::string(type) == "wayland";
}
#endif

static bool tryInitGlfw(int platform) {
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND) && defined(GLFW_PLATFORM_X11)
    if (platform != 0) glfwInitHint(GLFW_PLATFORM, platform);
#else
    (void) platform;
#endif

    if (glfwInit() == GLFW_TRUE) return true;
    glfwTerminate();
    return false;
}

static GLFWwindow *tryCreateWindowWithHints(int width, int height, const char *title, bool preferEgl) {
    glfwDefaultWindowHints();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

#ifdef __linux__
#if defined(GLFW_CONTEXT_CREATION_API) && defined(GLFW_EGL_CONTEXT_API) && defined(GLFW_NATIVE_CONTEXT_API)
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, preferEgl ? GLFW_EGL_CONTEXT_API : GLFW_NATIVE_CONTEXT_API);
#else
    (void) preferEgl;
#endif
#else
    (void) preferEgl;
#endif

    GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window) return window;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

#ifdef __linux__
#if defined(GLFW_CONTEXT_CREATION_API) && defined(GLFW_EGL_CONTEXT_API) && defined(GLFW_NATIVE_CONTEXT_API)
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, preferEgl ? GLFW_EGL_CONTEXT_API : GLFW_NATIVE_CONTEXT_API);
#endif
#endif

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window) return window;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

#ifdef __linux__
#if defined(GLFW_CONTEXT_CREATION_API) && defined(GLFW_EGL_CONTEXT_API) && defined(GLFW_NATIVE_CONTEXT_API)
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, preferEgl ? GLFW_EGL_CONTEXT_API : GLFW_NATIVE_CONTEXT_API);
#endif
#endif

    return glfwCreateWindow(width, height, title, nullptr, nullptr);
}

static GLFWwindow *tryInitAndCreate(int platform, bool preferEgl, int width, int height, const char *title) {
    if (!tryInitGlfw(platform)) return nullptr;

    GLFWwindow *window = tryCreateWindowWithHints(width, height, title, preferEgl);
    if (window) return window;

    glfwTerminate();
    return nullptr;
}

static void initOpenGLOrThrow() {
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) throw std::runtime_error("gladLoadGLLoader failed");
    if (!glGetString(GL_VERSION)) throw std::runtime_error("glGetString(GL_VERSION) failed");
}

Window::Window(int width, int height, const char *title) : m_window(nullptr) {
#ifdef _WIN32
    m_window = tryInitAndCreate(0, false, width, height, title);
    if (!m_window) throw std::runtime_error("failed to create window");
#else
    bool forceX11     = envTruthy("FORCE_GLFW_X11") || envTruthy("FORCE_X11");
    bool forceWayland = envTruthy("FORCE_GLFW_WAYLAND") || envTruthy("FORCE_WAYLAND");
    bool wlSession    = isWaylandSession();

#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND) && defined(GLFW_PLATFORM_X11)
    bool canWayland = glfwPlatformSupported(GLFW_PLATFORM_WAYLAND);
    bool canX11     = glfwPlatformSupported(GLFW_PLATFORM_X11);

    if (forceWayland) {
        if (canWayland) m_window = tryInitAndCreate(GLFW_PLATFORM_WAYLAND, true, width, height, title);
        if (!m_window && canX11) m_window = tryInitAndCreate(GLFW_PLATFORM_X11, false, width, height, title);
    } else if (forceX11) {
        if (canX11) m_window = tryInitAndCreate(GLFW_PLATFORM_X11, false, width, height, title);
        if (!m_window && canWayland) m_window = tryInitAndCreate(GLFW_PLATFORM_WAYLAND, true, width, height, title);
    } else if (wlSession) {
        if (canWayland) m_window = tryInitAndCreate(GLFW_PLATFORM_WAYLAND, true, width, height, title);
        if (!m_window && canX11) m_window = tryInitAndCreate(GLFW_PLATFORM_X11, false, width, height, title);
    } else {
        if (canX11) m_window = tryInitAndCreate(GLFW_PLATFORM_X11, false, width, height, title);
        if (!m_window && canWayland) m_window = tryInitAndCreate(GLFW_PLATFORM_WAYLAND, true, width, height, title);
    }

    if (!m_window) m_window = tryInitAndCreate(0, false, width, height, title);
#else
    m_window = tryInitAndCreate(0, false, width, height, title);
#endif

    if (!m_window) throw std::runtime_error("failed to create window");
#endif

    glfwMakeContextCurrent(m_window);
    initOpenGLOrThrow();
    glfwSwapInterval(1);
}

Window::~Window() {
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }

void Window::pollEvents() const { glfwPollEvents(); }

void Window::swapBuffers() const { glfwSwapBuffers(m_window); }

GLFWwindow *Window::getHandle() const { return m_window; }
