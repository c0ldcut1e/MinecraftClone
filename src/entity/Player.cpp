#include "Player.h"

Player::Player(World *world) : LivingEntity(world), m_moveForward(false), m_moveBackward(false), m_moveLeft(false), m_moveRight(false) {}

uint64_t Player::getType() { return TYPE; }

void Player::tick() {
    Vec3 forward = m_flying ? m_front : Vec3(m_front.x, 0.0, m_front.z);
    Vec3 right(m_right.x, 0.0, m_right.z);

    Vec3 direction(0.0, 0.0, 0.0);

    if (m_moveForward) direction = direction.add(forward);
    if (m_moveBackward) direction = direction.sub(forward);
    if (m_moveLeft) direction = direction.sub(right);
    if (m_moveRight) direction = direction.add(right);

    if (direction.x != 0.0 || direction.z != 0.0) setMoveIntent(direction.normalize());

    if (m_flying) {
        setNoGravity(true);
        setNoCollision(true);
    } else {
        setNoGravity(false);
        setNoCollision(false);
    }

    Entity::tick();
}

void Player::setMoveForward(bool value) { m_moveForward = value; }

void Player::setMoveBackward(bool value) { m_moveBackward = value; }

void Player::setMoveLeft(bool value) { m_moveLeft = value; }

void Player::setMoveRight(bool value) { m_moveRight = value; }

void Player::setFlying(bool value) { m_flying = value; }

bool Player::isFlying() const { return m_flying; }
