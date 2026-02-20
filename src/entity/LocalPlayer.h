#pragma once

#include "../scene/Camera.h"
#include "../world/block/BlockPos.h"
#include "Player.h"

class LocalPlayer : public Player {
public:
    LocalPlayer(World *world, const std::wstring &name, Camera *camera);

    uint64_t getType() override;

    void update(float partialTicks);
    void tick() override;

    void onKeyPressed(int key);
    void onKeyReleased(int key);
    void onMouseMoved(double dx, double dy);
    void onMouseButtonPressed(int button);

    static constexpr uint64_t TYPE = 0x3000000000000001;

private:
    void handleLMB();
    void handleRMB();

    void destroyBlock(const BlockPos &pos);
    void placeBlock(const BlockPos &pos, Direction *face, Block *block);

    void updateCamera(float partialTicks);

    Camera *m_camera;
    float m_mouseSensitivity;

    bool m_jumpHeld;
};
