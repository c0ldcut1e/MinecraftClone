#pragma once

#include <array>
#include <memory>

#include "ControllerBackend.h"

class InputManager
{
public:
    explicit InputManager(std::unique_ptr<ControllerBackend> controllerBackend);
    ~InputManager();

    void update(double deltaTime);
    void clear();

    void onKeyPressed(int key);
    void onKeyReleased(int key);
    void onMouseButtonPressed(int button);
    void onMouseMoved(double dx, double dy);

    bool isActionDown(GameplayAction action) const;
    bool consumeActionPress(GameplayAction action);

    float getMoveForward() const;
    float getMoveStrafe() const;
    void consumeLookDelta(double *dx, double *dy);
    bool consumeUiNavigateUpPress();
    bool consumeUiNavigateDownPress();
    bool consumeUiNavigateLeftPress();
    bool consumeUiNavigateRightPress();
    bool consumeUiActivatePress();
    void setControllerLookSensitivity(double sensitivity);
    double getControllerLookSensitivity() const;

private:
    void queueActionPress(GameplayAction action);

    bool m_keyboardMoveForward;
    bool m_keyboardMoveBackward;
    bool m_keyboardMoveLeft;
    bool m_keyboardMoveRight;
    bool m_keyboardJump;

    float m_controllerMoveForward;
    float m_controllerMoveStrafe;

    double m_lookDeltaX;
    double m_lookDeltaY;
    bool m_pendingUiNavigateUpPress;
    bool m_pendingUiNavigateDownPress;
    bool m_pendingUiNavigateLeftPress;
    bool m_pendingUiNavigateRightPress;
    bool m_pendingUiActivatePress;

    std::array<bool, GAMEPLAY_ACTION_COUNT> m_controllerActionDown;
    std::array<bool, GAMEPLAY_ACTION_COUNT> m_pendingActionPresses;

    std::unique_ptr<ControllerBackend> m_controllerBackend;
};
