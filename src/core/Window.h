#pragma once

#include <SDL2/SDL.h>

class Window
{
public:
    Window(int width, int height, const char *title);
    ~Window();

    void requestClose();
    bool shouldClose() const;

    void swapBuffers() const;

    void enableVSync() const;
    void disableVSync() const;

    SDL_Window *getHandle() const;
    SDL_GLContext getGLContext() const;

private:
    SDL_Window *m_window;
    SDL_GLContext m_glContext;
    bool m_shouldClose;
};
