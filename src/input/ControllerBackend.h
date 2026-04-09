#pragma once

#include <array>

#include "GameplayAction.h"

struct ControllerState
{
    float moveForward;
    float moveStrafe;
    double lookDeltaX;
    double lookDeltaY;
    std::array<bool, GAMEPLAY_ACTION_COUNT> actionDown;
    std::array<bool, GAMEPLAY_ACTION_COUNT> actionPressed;
    bool uiNavigateUpPressed;
    bool uiNavigateDownPressed;
    bool uiNavigateLeftPressed;
    bool uiNavigateRightPressed;
    bool uiActivatePressed;

    ControllerState();
};

class ControllerBackend
{
public:
    virtual ~ControllerBackend() = default;

    virtual void update(ControllerState &state, double deltaTime) = 0;
    virtual void setLookSensitivity(double sensitivity);
    virtual double getLookSensitivity() const;
};
