#pragma once

#include <array>
#include <cstdint>

#include <SDL2/SDL.h>

#include "ControllerBackend.h"

class SdlControllerBackend : public ControllerBackend
{
public:
    SdlControllerBackend();
    ~SdlControllerBackend() override;

    void update(ControllerState &state, double deltaTime) override;
    void setLookSensitivity(double sensitivity) override;
    double getLookSensitivity() const override;

private:
    bool ensureController();
    void closeController();

    static float normalizeAxis(int value, int deadzone);
    static float normalizeTrigger(int value, int deadzone);

    SDL_GameController *m_controller;
    std::array<bool, GAMEPLAY_ACTION_COUNT> m_previousActionDown;
    bool m_previousUiNavigateUpDown;
    bool m_previousUiNavigateDownDown;
    bool m_previousUiNavigateLeftDown;
    bool m_previousUiNavigateRightDown;
    bool m_previousActivateDown;
    uint32_t m_nextOpenAttemptTicks;
    double m_lookSensitivity;
};
