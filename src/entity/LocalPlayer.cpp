#include "LocalPlayer.h"

#include <cmath>

#include <GLFW/glfw3.h>

#include "../core/Minecraft.h"
#include "../world/WorldRenderer.h"
#include "../world/block/Block.h"
#include "../world/lighting/LightEngine.h"

LocalPlayer::LocalPlayer(World *world, const std::wstring &name, Camera *camera) : Player(world, name), m_camera(camera), m_mouseSensitivity(0.08f), m_jumpHeld(false) {}

uint64_t LocalPlayer::getType() { return TYPE; }

void LocalPlayer::tick() {
    if (m_jumpHeld) queueJump();

    Player::tick();

    if (m_camera) {
        m_camera->setPosition(m_position.add(Vec3(0.0, 1.62, 0.0)));
        m_camera->setDirection(m_front, m_up);
    }
}

void LocalPlayer::onKeyPressed(int key) {
    if (key == GLFW_KEY_W) setMoveForward(true);
    if (key == GLFW_KEY_S) setMoveBackward(true);
    if (key == GLFW_KEY_A) setMoveLeft(true);
    if (key == GLFW_KEY_D) setMoveRight(true);
    if (key == GLFW_KEY_SPACE) m_jumpHeld = true;
    if (key == GLFW_KEY_F) setFlying(!m_flying);
}

void LocalPlayer::onKeyReleased(int key) {
    if (key == GLFW_KEY_W) setMoveForward(false);
    if (key == GLFW_KEY_S) setMoveBackward(false);
    if (key == GLFW_KEY_A) setMoveLeft(false);
    if (key == GLFW_KEY_D) setMoveRight(false);
    if (key == GLFW_KEY_SPACE) m_jumpHeld = false;
}

void LocalPlayer::onMouseMoved(double dx, double dy) {
    m_yaw += (float) dx * m_mouseSensitivity;
    m_pitch += (float) dy * m_mouseSensitivity;
    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;
}

void LocalPlayer::onMouseButtonPressed(int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
        Vec3 direction = m_front;
        int x;
        int y;
        int z;
        int face;
        if (m_world->clip(origin, direction, 6.0f, &x, &y, &z, &face)) m_world->setBlock(x, y, z, Block::byId(0));
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
        Vec3 direction = m_front.normalize();

        int x;
        int y;
        int z;
        int face;
        if (m_world->clip(origin, direction, 6.0f, &x, &y, &z, &face)) {
            int placeX = x;
            int placeY = y;
            int placeZ = z;
            if (face == 0) placeX--;
            if (face == 1) placeX++;
            if (face == 2) placeY--;
            if (face == 3) placeY++;
            if (face == 4) placeZ--;
            if (face == 5) placeZ++;

            AABB blockBox(Vec3(placeX, placeY, placeZ), Vec3(placeX + 1, placeY + 1, placeZ + 1));
            if (!blockBox.intersects(getAABB())) m_world->setBlock(placeX, placeY, placeZ, Block::byName("glowstone"));
        }
    }
}
