#include "ControllerBackend.h"

ControllerState::ControllerState()
    : moveForward(0.0f), moveStrafe(0.0f), lookDeltaX(0.0), lookDeltaY(0.0), actionDown(),
      actionPressed(), uiNavigateUpPressed(false), uiNavigateDownPressed(false),
      uiNavigateLeftPressed(false), uiNavigateRightPressed(false), uiActivatePressed(false)
{}

void ControllerBackend::setLookSensitivity(double) {}

double ControllerBackend::getLookSensitivity() const { return 100.0; }
