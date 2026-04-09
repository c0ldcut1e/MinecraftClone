#include "InputManager.h"

#include <algorithm>

#include <SDL2/SDL.h>

#include "../utils/math/Mth.h"

static size_t gameplayActionIndex(GameplayAction action) { return (size_t) action; }

InputManager::InputManager(std::unique_ptr<ControllerBackend> controllerBackend)
    : m_keyboardMoveForward(false), m_keyboardMoveBackward(false), m_keyboardMoveLeft(false),
      m_keyboardMoveRight(false), m_keyboardJump(false), m_controllerMoveForward(0.0f),
      m_controllerMoveStrafe(0.0f), m_lookDeltaX(0.0), m_lookDeltaY(0.0),
      m_pendingUiNavigateUpPress(false), m_pendingUiNavigateDownPress(false),
      m_pendingUiNavigateLeftPress(false), m_pendingUiNavigateRightPress(false),
      m_pendingUiActivatePress(false), m_controllerActionDown(), m_pendingActionPresses(),
      m_controllerBackend(std::move(controllerBackend))
{}

InputManager::~InputManager() {}

void InputManager::update(double deltaTime)
{
    m_controllerMoveForward = 0.0f;
    m_controllerMoveStrafe  = 0.0f;
    m_controllerActionDown.fill(false);

    if (!m_controllerBackend)
    {
        return;
    }

    ControllerState state;
    m_controllerBackend->update(state, deltaTime);

    m_controllerMoveForward = state.moveForward;
    m_controllerMoveStrafe  = state.moveStrafe;
    m_lookDeltaX += state.lookDeltaX;
    m_lookDeltaY += state.lookDeltaY;
    if (state.uiNavigateUpPressed)
    {
        m_pendingUiNavigateUpPress = true;
    }
    if (state.uiNavigateDownPressed)
    {
        m_pendingUiNavigateDownPress = true;
    }
    if (state.uiNavigateLeftPressed)
    {
        m_pendingUiNavigateLeftPress = true;
    }
    if (state.uiNavigateRightPressed)
    {
        m_pendingUiNavigateRightPress = true;
    }
    if (state.uiActivatePressed)
    {
        m_pendingUiActivatePress = true;
    }

    for (size_t i = 0; i < GAMEPLAY_ACTION_COUNT; i++)
    {
        m_controllerActionDown[i] = state.actionDown[i];
        if (state.actionPressed[i])
        {
            m_pendingActionPresses[i] = true;
        }
    }
}

void InputManager::clear()
{
    m_keyboardMoveForward  = false;
    m_keyboardMoveBackward = false;
    m_keyboardMoveLeft     = false;
    m_keyboardMoveRight    = false;
    m_keyboardJump         = false;

    m_controllerMoveForward = 0.0f;
    m_controllerMoveStrafe  = 0.0f;

    m_lookDeltaX                  = 0.0;
    m_lookDeltaY                  = 0.0;
    m_pendingUiNavigateUpPress    = false;
    m_pendingUiNavigateDownPress  = false;
    m_pendingUiNavigateLeftPress  = false;
    m_pendingUiNavigateRightPress = false;
    m_pendingUiActivatePress      = false;

    m_controllerActionDown.fill(false);
    m_pendingActionPresses.fill(false);
}

void InputManager::onKeyPressed(int key)
{
    if (key == SDL_SCANCODE_W)
    {
        m_keyboardMoveForward = true;
    }
    if (key == SDL_SCANCODE_S)
    {
        m_keyboardMoveBackward = true;
    }
    if (key == SDL_SCANCODE_A)
    {
        m_keyboardMoveLeft = true;
    }
    if (key == SDL_SCANCODE_D)
    {
        m_keyboardMoveRight = true;
    }
    if (key == SDL_SCANCODE_SPACE)
    {
        m_keyboardJump = true;
    }
    if (key == SDL_SCANCODE_F)
    {
        queueActionPress(GameplayAction::ToggleFly);
    }
    if (key == SDL_SCANCODE_UP)
    {
        m_pendingUiNavigateUpPress = true;
    }
    if (key == SDL_SCANCODE_DOWN)
    {
        m_pendingUiNavigateDownPress = true;
    }
    if (key == SDL_SCANCODE_LEFT)
    {
        m_pendingUiNavigateLeftPress = true;
    }
    if (key == SDL_SCANCODE_RIGHT)
    {
        m_pendingUiNavigateRightPress = true;
    }
    if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER)
    {
        m_pendingUiActivatePress = true;
    }
}

void InputManager::onKeyReleased(int key)
{
    if (key == SDL_SCANCODE_W)
    {
        m_keyboardMoveForward = false;
    }
    if (key == SDL_SCANCODE_S)
    {
        m_keyboardMoveBackward = false;
    }
    if (key == SDL_SCANCODE_A)
    {
        m_keyboardMoveLeft = false;
    }
    if (key == SDL_SCANCODE_D)
    {
        m_keyboardMoveRight = false;
    }
    if (key == SDL_SCANCODE_SPACE)
    {
        m_keyboardJump = false;
    }
}

void InputManager::onMouseButtonPressed(int button)
{
    if (button == SDL_BUTTON_LEFT)
    {
        queueActionPress(GameplayAction::BreakBlock);
    }
    if (button == SDL_BUTTON_RIGHT)
    {
        queueActionPress(GameplayAction::PlaceBlock);
    }
}

void InputManager::onMouseMoved(double dx, double dy)
{
    m_lookDeltaX += dx;
    m_lookDeltaY += dy;
}

bool InputManager::isActionDown(GameplayAction action) const
{
    if (action == GameplayAction::Jump)
    {
        return m_keyboardJump || m_controllerActionDown[gameplayActionIndex(action)];
    }

    return m_controllerActionDown[gameplayActionIndex(action)];
}

bool InputManager::consumeActionPress(GameplayAction action)
{
    size_t index                  = gameplayActionIndex(action);
    bool pressed                  = m_pendingActionPresses[index];
    m_pendingActionPresses[index] = false;
    return pressed;
}

float InputManager::getMoveForward() const
{
    float keyboard = 0.0f;
    if (m_keyboardMoveForward)
    {
        keyboard += 1.0f;
    }
    if (m_keyboardMoveBackward)
    {
        keyboard -= 1.0f;
    }

    return Mth::clampf(keyboard + m_controllerMoveForward, -1.0f, 1.0f);
}

float InputManager::getMoveStrafe() const
{
    float keyboard = 0.0f;
    if (m_keyboardMoveRight)
    {
        keyboard += 1.0f;
    }
    if (m_keyboardMoveLeft)
    {
        keyboard -= 1.0f;
    }

    return Mth::clampf(keyboard + m_controllerMoveStrafe, -1.0f, 1.0f);
}

void InputManager::consumeLookDelta(double *dx, double *dy)
{
    if (dx)
    {
        *dx = m_lookDeltaX;
    }
    if (dy)
    {
        *dy = m_lookDeltaY;
    }

    m_lookDeltaX = 0.0;
    m_lookDeltaY = 0.0;
}

bool InputManager::consumeUiNavigateUpPress()
{
    bool pressed               = m_pendingUiNavigateUpPress;
    m_pendingUiNavigateUpPress = false;
    return pressed;
}

bool InputManager::consumeUiNavigateDownPress()
{
    bool pressed                 = m_pendingUiNavigateDownPress;
    m_pendingUiNavigateDownPress = false;
    return pressed;
}

bool InputManager::consumeUiNavigateLeftPress()
{
    bool pressed                 = m_pendingUiNavigateLeftPress;
    m_pendingUiNavigateLeftPress = false;
    return pressed;
}

bool InputManager::consumeUiNavigateRightPress()
{
    bool pressed                  = m_pendingUiNavigateRightPress;
    m_pendingUiNavigateRightPress = false;
    return pressed;
}

bool InputManager::consumeUiActivatePress()
{
    bool pressed             = m_pendingUiActivatePress;
    m_pendingUiActivatePress = false;
    return pressed;
}

void InputManager::setControllerLookSensitivity(double sensitivity)
{
    if (m_controllerBackend)
    {
        m_controllerBackend->setLookSensitivity(sensitivity);
    }
}

double InputManager::getControllerLookSensitivity() const
{
    if (m_controllerBackend)
    {
        return m_controllerBackend->getLookSensitivity();
    }

    return 100.0;
}

void InputManager::queueActionPress(GameplayAction action)
{
    m_pendingActionPresses[gameplayActionIndex(action)] = true;
}
