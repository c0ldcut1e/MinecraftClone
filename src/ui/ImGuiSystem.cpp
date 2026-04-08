#include "ImGuiSystem.h"

#include <SDL2/SDL.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_sdl2.h"

ImGuiSystem::ImGuiSystem() : m_initialized(false), m_visible(false) {}

ImGuiSystem::~ImGuiSystem() { shutdown(); }

void ImGuiSystem::init(void *windowHandle)
{
    if (m_initialized || !windowHandle)
    {
        return;
    }

    SDL_Window *window = static_cast<SDL_Window *>(windowHandle);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext());
    ImGui_ImplOpenGL3_Init("#version 460 core");

    m_initialized = true;
}

void ImGuiSystem::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
}

void ImGuiSystem::beginFrame()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void ImGuiSystem::render()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiSystem::setVisible(bool visible) { m_visible = visible; }

void ImGuiSystem::toggleVisible() { m_visible = !m_visible; }

bool ImGuiSystem::isVisible() const { return m_visible; }

bool ImGuiSystem::wantsKeyboardCapture() const
{
    return m_initialized && ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiSystem::wantsMouseCapture() const
{
    return m_initialized && ImGui::GetIO().WantCaptureMouse;
}
