#include "Player.h"

#include <cmath>

Player::Player(Level *level, const std::wstring &name)
    : LivingEntity(level), m_moveForwardInput(0.0f), m_moveStrafeInput(0.0f)
{
    m_name = name;
}

uint64_t Player::getType() { return TYPE; }

void Player::tick()
{
    Vec3 forward = m_flying ? m_front : Vec3(m_front.x, 0.0, m_front.z);
    Vec3 right(m_right.x, 0.0, m_right.z);

    Vec3 direction(0.0, 0.0, 0.0);

    if (m_moveForwardInput != 0.0f)
    {
        direction = direction.add(forward.scale(m_moveForwardInput));
    }
    if (m_moveStrafeInput != 0.0f)
    {
        direction = direction.add(right.scale(m_moveStrafeInput));
    }

    double directionLength =
            sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (directionLength > 1.0)
    {
        direction = direction.scale(1.0 / directionLength);
    }

    if (direction.x != 0.0 || direction.y != 0.0 || direction.z != 0.0)
    {
        setMoveIntent(direction);
    }

    if (m_flying)
    {
        setNoGravity(true);
        setNoCollision(true);
    }
    else
    {
        setNoGravity(false);
        setNoCollision(false);
    }

    Entity::tick();
}

void Player::setMoveInputs(float forward, float strafe)
{
    m_moveForwardInput = forward;
    m_moveStrafeInput  = strafe;
}

void Player::setFlying(bool value) { m_flying = value; }

bool Player::isFlying() const { return m_flying; }
