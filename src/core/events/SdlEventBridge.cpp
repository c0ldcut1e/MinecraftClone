#include "SdlEventBridge.h"

#include <SDL2/SDL.h>

#include "../../ui/imgui/imgui_impl_sdl2.h"
#include "EventManager.h"
#include "KeyPressedEvent.h"
#include "KeyReleasedEvent.h"
#include "MouseButtonPressedEvent.h"
#include "MouseMovedEvent.h"
#include "WindowCloseEvent.h"
#include "WindowResizedEvent.h"

static bool s_first         = true;
static SDL_Window *s_window = nullptr;

void initSdlEventBridge(void *windowHandle) { s_window = (SDL_Window *) windowHandle; }

void resetSdlMouseState() { s_first = true; }

void pumpSdlEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type)
        {
            case SDL_QUIT: {
                EventManager::push(new WindowCloseEvent());
                break;
            }

            case SDL_KEYDOWN: {
                if (event.key.repeat == 0)
                {
                    EventManager::push(new KeyPressedEvent(event.key.keysym.scancode));
                }
                break;
            }

            case SDL_KEYUP: {
                EventManager::push(new KeyReleasedEvent(event.key.keysym.scancode));
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                EventManager::push(new MouseButtonPressedEvent(event.button.button));
                break;
            }

            case SDL_MOUSEMOTION: {
                if (s_first)
                {
                    s_first = false;
                    break;
                }

                EventManager::push(new MouseMovedEvent((double) event.motion.xrel,
                                                       (double) -event.motion.yrel));
                break;
            }

            case SDL_WINDOWEVENT: {
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    int width  = event.window.data1;
                    int height = event.window.data2;

                    if (width > 0 && height > 0)
                    {
                        EventManager::push(new WindowResizedEvent(width, height));
                    }
                }
                break;
            }

            default: {
                break;
            }
        }
    }
}
