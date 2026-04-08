#include "LocalPlayer.h"

#include <cmath>

#include <GLFW/glfw3.h>

#include "../core/Minecraft.h"
#include "../world/LevelRenderer.h"
#include "../world/block/Block.h"
#include "../world/lighting/LightEngine.h"

LocalPlayer::LocalPlayer(Level *level, const std::wstring &name, Camera *camera)
    : Player(level, name), m_camera(camera), m_mouseSensitivity(0.08f), m_jumpHeld(false)
{}

uint64_t LocalPlayer::getType() { return TYPE; }

void LocalPlayer::update(float partialTicks) { updateCamera(partialTicks); }

void LocalPlayer::tick()
{
    if (m_jumpHeld)
    {
        jump();
    }

    Player::tick();
}

void LocalPlayer::onKeyPressed(int key)
{
    if (key == SDL_SCANCODE_W)
    {
        setMoveForward(true);
    }
    if (key == SDL_SCANCODE_S)
    {
        setMoveBackward(true);
    }
    if (key == SDL_SCANCODE_A)
    {
        setMoveLeft(true);
    }
    if (key == SDL_SCANCODE_D)
    {
        setMoveRight(true);
    }
    if (key == SDL_SCANCODE_SPACE)
    {
        m_jumpHeld = true;
    }
    if (key == SDL_SCANCODE_F)
    {
        setFlying(!m_flying);
    }
}

void LocalPlayer::onKeyReleased(int key)
{
    if (key == SDL_SCANCODE_W)
    {
        setMoveForward(false);
    }
    if (key == SDL_SCANCODE_S)
    {
        setMoveBackward(false);
    }
    if (key == SDL_SCANCODE_A)
    {
        setMoveLeft(false);
    }
    if (key == SDL_SCANCODE_D)
    {
        setMoveRight(false);
    }
    if (key == SDL_SCANCODE_SPACE)
    {
        m_jumpHeld = false;
    }
}

void LocalPlayer::onMouseMoved(double dx, double dy)
{
    dx *= m_mouseSensitivity;
    dy *= m_mouseSensitivity;

    m_yaw += dx;
    m_pitch += dy;

    if (m_pitch < -89.0f)
    {
        m_pitch = -89.0f;
    }
    if (m_pitch > 89.0f)
    {
        m_pitch = 89.0f;
    }

    updateVectors();

    if (m_camera)
    {
        m_camera->setDirection(m_front, m_up);
    }
}

void LocalPlayer::onMouseButtonPressed(int button)
{
    if (button == SDL_BUTTON_LEFT)
    {
        handleLMB();
    }
    if (button == SDL_BUTTON_RIGHT)
    {
        handleRMB();
    }
}

void LocalPlayer::clearInputs()
{
    setMoveForward(false);
    setMoveBackward(false);
    setMoveLeft(false);
    setMoveRight(false);
    m_jumpHeld = false;
}

void LocalPlayer::handleLMB()
{
    Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
    Vec3 direction = m_front.normalize();
    HitResult *hit = m_level->clip(origin, direction, 6.0f);
    if (hit->isBlock())
    {
        destroyBlock(hit->getBlockPos());
    }

    delete hit;
}

void LocalPlayer::handleRMB()
{
    Vec3 origin    = m_position.add(Vec3(0.0, 1.62, 0.0));
    Vec3 direction = m_front.normalize();
    HitResult *hit = m_level->clip(origin, direction, 6.0f);
    if (hit->isBlock())
    {
        placeBlock(hit->getBlockPos(), hit->getBlockFace(), Block::byName("torch"));
    }

    delete hit;
}

void LocalPlayer::destroyBlock(const BlockPos &pos) { m_level->setBlock(pos, Block::byId(0)); }

void LocalPlayer::placeBlock(const BlockPos &pos, Direction *face, Block *block)
{
    BlockPos placePos = pos;
    if (face == Direction::WEST)
    {
        placePos.x--;
    }
    if (face == Direction::EAST)
    {
        placePos.x++;
    }
    if (face == Direction::DOWN)
    {
        placePos.y--;
    }
    if (face == Direction::UP)
    {
        placePos.y++;
    }
    if (face == Direction::NORTH)
    {
        placePos.z--;
    }
    if (face == Direction::SOUTH)
    {
        placePos.z++;
    }

    AABB blockBox(Vec3(placePos.x, placePos.y, placePos.z),
                  Vec3(placePos.x + 1.0f, placePos.y + 1.0f, placePos.z + 1.0f));
    if (!blockBox.intersects(getAABB()))
    {
        m_level->setBlock(placePos, block, face);
    }
}

void LocalPlayer::updateCamera(float partialTicks)
{
    if (!m_camera)
    {
        return;
    }

    Vec3 pos = getRenderPosition(partialTicks);
    m_camera->setPosition(pos.add(Vec3(0.0, 1.62, 0.0)));
    m_camera->setDirection(m_front, m_up);
}
