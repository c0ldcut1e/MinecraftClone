#include "SdlControllerBackend.h"

#include <algorithm>
#include <cmath>

#include "../core/Logger.h"
#include "../utils/math/Mth.h"

static size_t gameplayActionIndex(GameplayAction action) { return (size_t) action; }

SdlControllerBackend::SdlControllerBackend()
    : m_controller(nullptr), m_previousActionDown(), m_previousUiNavigateUpDown(false),
      m_previousUiNavigateDownDown(false), m_previousUiNavigateLeftDown(false),
      m_previousUiNavigateRightDown(false), m_previousActivateDown(false),
      m_nextOpenAttemptTicks(0), m_lookSensitivity(130.0)
{}

SdlControllerBackend::~SdlControllerBackend() { closeController(); }

void SdlControllerBackend::update(ControllerState &state, double deltaTime)
{
    if (!ensureController())
    {
        m_previousActionDown.fill(false);
        m_previousUiNavigateUpDown    = false;
        m_previousUiNavigateDownDown  = false;
        m_previousUiNavigateLeftDown  = false;
        m_previousUiNavigateRightDown = false;
        m_previousActivateDown        = false;
        return;
    }

    SDL_GameControllerUpdate();

    constexpr int leftStickDeadzone  = 8000;
    constexpr int rightStickDeadzone = 8000;
    constexpr int triggerDeadzone    = 12000;
    constexpr double minLookSpeed    = 1.0;
    constexpr double maxLookSpeed    = 4000.0;
    constexpr double maxSensitivity  = 200.0;
    constexpr float triggerThreshold = 0.5f;
    double normalizedSensitivity     = Mth::clampf(m_lookSensitivity / maxSensitivity, 0.0, 1.0);
    double lookSpeed                 = Mth::lerp(minLookSpeed, maxLookSpeed, normalizedSensitivity);

    state.moveStrafe = normalizeAxis(
            SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX), leftStickDeadzone);
    state.moveForward = -normalizeAxis(
            SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY), leftStickDeadzone);
    state.lookDeltaX +=
            normalizeAxis(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTX),
                          rightStickDeadzone) *
            lookSpeed * deltaTime;
    state.lookDeltaY +=
            -normalizeAxis(SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_RIGHTY),
                           rightStickDeadzone) *
            lookSpeed * deltaTime;

    std::array<bool, GAMEPLAY_ACTION_COUNT> currentActionDown{};
    currentActionDown[gameplayActionIndex(GameplayAction::Jump)] =
            SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_B) != 0;
    currentActionDown[gameplayActionIndex(GameplayAction::ToggleFly)] =
            SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_X) != 0;
    currentActionDown[gameplayActionIndex(GameplayAction::BreakBlock)] =
            normalizeTrigger(
                    SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT),
                    triggerDeadzone) >= triggerThreshold;
    currentActionDown[gameplayActionIndex(GameplayAction::PlaceBlock)] =
            normalizeTrigger(
                    SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                    triggerDeadzone) >= triggerThreshold;
    bool dpadUpDown = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool dpadDownDown =
            SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool dpadLeftDown =
            SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
    bool dpadRightDown =
            SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
    float leftStickNavigate = normalizeAxis(
            SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTY), leftStickDeadzone);
    float leftStickHorizontal = normalizeAxis(
            SDL_GameControllerGetAxis(m_controller, SDL_CONTROLLER_AXIS_LEFTX), leftStickDeadzone);
    bool stickUpDown       = leftStickNavigate <= -0.5f;
    bool stickDownDown     = leftStickNavigate >= 0.5f;
    bool stickLeftDown     = leftStickHorizontal <= -0.5f;
    bool stickRightDown    = leftStickHorizontal >= 0.5f;
    bool navigateUpDown    = dpadUpDown || stickUpDown;
    bool navigateDownDown  = dpadDownDown || stickDownDown;
    bool navigateLeftDown  = dpadLeftDown || stickLeftDown;
    bool navigateRightDown = dpadRightDown || stickRightDown;
    bool activateDown = SDL_GameControllerGetButton(m_controller, SDL_CONTROLLER_BUTTON_B) != 0;
    state.uiNavigateUpPressed    = navigateUpDown && !m_previousUiNavigateUpDown;
    state.uiNavigateDownPressed  = navigateDownDown && !m_previousUiNavigateDownDown;
    state.uiNavigateLeftPressed  = navigateLeftDown && !m_previousUiNavigateLeftDown;
    state.uiNavigateRightPressed = navigateRightDown && !m_previousUiNavigateRightDown;
    state.uiActivatePressed      = activateDown && !m_previousActivateDown;

    for (size_t i = 0; i < GAMEPLAY_ACTION_COUNT; i++)
    {
        state.actionDown[i]    = currentActionDown[i];
        state.actionPressed[i] = currentActionDown[i] && !m_previousActionDown[i];
    }

    m_previousActionDown          = currentActionDown;
    m_previousUiNavigateUpDown    = navigateUpDown;
    m_previousUiNavigateDownDown  = navigateDownDown;
    m_previousUiNavigateLeftDown  = navigateLeftDown;
    m_previousUiNavigateRightDown = navigateRightDown;
    m_previousActivateDown        = activateDown;
}

void SdlControllerBackend::setLookSensitivity(double sensitivity)
{
    m_lookSensitivity = Mth::clampf(sensitivity, 0.0, 200.0);
}

double SdlControllerBackend::getLookSensitivity() const { return m_lookSensitivity; }

bool SdlControllerBackend::ensureController()
{
    if (m_controller && SDL_GameControllerGetAttached(m_controller) == SDL_TRUE)
    {
        return true;
    }

    if (m_controller)
    {
        Logger::logInfo("Controller disconnected");
        closeController();
    }

    uint32_t now = SDL_GetTicks();
    if (now < m_nextOpenAttemptTicks)
    {
        return false;
    }

    int joystickCount = SDL_NumJoysticks();
    for (int i = 0; i < joystickCount; i++)
    {
        if (SDL_IsGameController(i) != SDL_TRUE)
        {
            continue;
        }

        m_controller = SDL_GameControllerOpen(i);
        if (m_controller)
        {
            const char *name = SDL_GameControllerName(m_controller);
            Logger::logInfo("Controller connected: %s", name ? name : "unknown");
            return true;
        }
    }

    m_nextOpenAttemptTicks = now + 1000;
    return false;
}

void SdlControllerBackend::closeController()
{
    if (m_controller)
    {
        SDL_GameControllerClose(m_controller);
        m_controller = nullptr;
    }

    m_previousActionDown.fill(false);
    m_previousUiNavigateUpDown    = false;
    m_previousUiNavigateDownDown  = false;
    m_previousUiNavigateLeftDown  = false;
    m_previousUiNavigateRightDown = false;
    m_previousActivateDown        = false;
}

float SdlControllerBackend::normalizeAxis(int value, int deadzone)
{
    float normalized = (float) value / 32767.0f;
    float magnitude  = fabsf(normalized);
    if (magnitude <= (float) deadzone / 32767.0f)
    {
        return 0.0f;
    }

    float sign      = normalized < 0.0f ? -1.0f : 1.0f;
    float threshold = (float) deadzone / 32767.0f;
    float scaled    = (magnitude - threshold) / (1.0f - threshold);
    return sign * Mth::clampf(scaled, 0.0f, 1.0f);
}

float SdlControllerBackend::normalizeTrigger(int value, int deadzone)
{
    float normalized = (float) value / 32767.0f;
    float threshold  = (float) deadzone / 32767.0f;
    if (normalized <= threshold)
    {
        return 0.0f;
    }

    return Mth::clampf((normalized - threshold) / (1.0f - threshold), 0.0f, 1.0f);
}
