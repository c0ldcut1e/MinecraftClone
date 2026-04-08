#include "Window.h"

#include <stdexcept>
#include <string>

#include <glad/glad.h>

static void initOpenGLOrThrow()
{
    if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress))
    {
        throw std::runtime_error("gladLoadGLLoader failed");
    }

    if (!glGetString(GL_VERSION))
    {
        throw std::runtime_error("glGetString(GL_VERSION) failed");
    }
}

Window::Window(int width, int height, const char *title)
    : m_window(nullptr), m_glContext(nullptr), m_shouldClose(false)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!m_window)
    {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext)
    {
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    }

    if (SDL_GL_MakeCurrent(m_window, m_glContext) != 0)
    {
        SDL_GL_DeleteContext(m_glContext);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
    }

    initOpenGLOrThrow();
    enableVSync();
}

Window::~Window()
{
    if (m_glContext)
    {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }

    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void Window::requestClose() { m_shouldClose = true; }

bool Window::shouldClose() const { return m_shouldClose; }

void Window::swapBuffers() const { SDL_GL_SwapWindow(m_window); }

void Window::enableVSync() const { SDL_GL_SetSwapInterval(1); }

void Window::disableVSync() const { SDL_GL_SetSwapInterval(0); }

SDL_Window *Window::getHandle() const { return m_window; }

SDL_GLContext Window::getGLContext() const { return m_glContext; }
