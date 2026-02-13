#pragma once

#include "../scene/Camera.h"
#include "Player.h"

class LocalPlayer : public Player {
public:
    LocalPlayer(World *world, Camera *camera);

    uint64_t getType() override;
    void tick() override;

    void onKeyPressed(int key);
    void onKeyReleased(int key);
    void onMouseMoved(double dx, double dy);
    void onMouseButtonPressed(int button);

    static constexpr uint64_t TYPE = 0x1000000000000004;

private:
    Camera *m_camera;
    float m_mouseSensitivity;

    bool m_jumpHeld;
};
