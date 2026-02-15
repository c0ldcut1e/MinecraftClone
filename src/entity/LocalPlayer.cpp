#include "LocalPlayer.h"

#include <cmath>

#include <GLFW/glfw3.h>

#include "../core/Minecraft.h"
#include "../world/WorldRenderer.h"
#include "../world/block/Block.h"
#include "../world/lighting/LightEngine.h"

LocalPlayer::LocalPlayer(World *world, const std::wstring &name, Camera *camera) : Player(world, name), m_camera(camera), m_mouseSensitivity(0.08f), m_jumpHeld(false) {}

uint64_t LocalPlayer::getType() { return TYPE; }

void LocalPlayer::update(float alpha) { updateCamera(alpha); }

void LocalPlayer::tick() {
    if (m_jumpHeld) jump();

    Player::tick();
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
    dx *= m_mouseSensitivity;
    dy *= m_mouseSensitivity;

    m_yaw += dx;
    m_pitch += dy;

    if (m_pitch < -89.0f) m_pitch = -89.0f;
    if (m_pitch > 89.0f) m_pitch = 89.0f;

    updateVectors();

    if (m_camera) m_camera->setDirection(m_front, m_up);
}

void LocalPlayer::onMouseButtonPressed(int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) handleLMB();
    if (button == GLFW_MOUSE_BUTTON_RIGHT) handleRMB();
}

void LocalPlayer::handleLMB() {
    Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
    Vec3 direction = m_front.normalize();
    HitResult *hit = m_world->clip(origin, direction, 6.0f);
    if (hit->isBlock()) destroyBlock(hit->getBlockPos());

    delete hit;
}

void LocalPlayer::handleRMB() {
    Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
    Vec3 direction = m_front.normalize();
    HitResult *hit = m_world->clip(origin, direction, 6.0f);
    if (hit->isBlock()) placeBlock(hit->getBlockPos(), hit->getBlockFace(), Block::byName("glowstone"));

    delete hit;
}

void LocalPlayer::destroyBlock(const BlockPos &pos) { m_world->setBlock(pos, Block::byId(0)); }

void LocalPlayer::placeBlock(const BlockPos &pos, Direction *face, Block *block) {
    BlockPos placePos = pos;
    if (face == Direction::WEST) placePos.x--;
    if (face == Direction::EAST) placePos.x++;
    if (face == Direction::DOWN) placePos.y--;
    if (face == Direction::UP) placePos.y++;
    if (face == Direction::NORTH) placePos.z--;
    if (face == Direction::SOUTH) placePos.z++;

    AABB blockBox(Vec3(placePos.x, placePos.y, placePos.z), Vec3(placePos.x + 1.0f, placePos.y + 1.0f, placePos.z + 1.0f));
    if (!blockBox.intersects(getAABB())) m_world->setBlock(placePos, block);
}

void LocalPlayer::updateCamera(float alpha) {
    if (!m_camera) return;

    Vec3 pos = getRenderPosition(alpha);
    m_camera->setPosition(pos.add(Vec3(0.0, 1.62, 0.0)));
    m_camera->setDirection(m_front, m_up);
}
