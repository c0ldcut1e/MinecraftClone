#include "Entity.h"

#include <cmath>

#include "../utils/Time.h"

Entity::Entity(World *world)
    : m_world(world), m_position(0.0, 0.0, 0.0), m_velocity(0.0, 0.0, 0.0), m_moveIntent(0.0, 0.0, 0.0), m_front(0.0, 0.0, -1.0), m_up(0.0, 1.0, 0.0), m_yaw(-90.0f), m_pitch(0.0f), m_gravity(-32.0f), m_jumpVelocity(9.2f), m_maxSpeed(15.0f),
      m_acceleration(80.0f), m_friction(10.0f), m_onGround(false), m_jumpQueued(false), m_noGravity(false), m_noCollision(false), m_flying(false), m_boundingBox(Vec3(-0.3, 0.0, -0.3), Vec3(0.3, 1.8, 0.3)), m_aabb() {}

Entity::~Entity() {}

uint64_t Entity::getType() { return TYPE; }

void Entity::tick() {
    updateVectors();
    updateAABB();

    applyPhysics(Time::delta());

    updateAABB();

    m_moveIntent = Vec3(0.0, 0.0, 0.0);
    m_jumpQueued = false;
}

void Entity::setPosition(const Vec3 &position) { m_position = position; }

const Vec3 &Entity::getPosition() const { return m_position; }

const Vec3 &Entity::getFront() const { return m_front; }

float Entity::getYaw() const { return m_yaw; }

float Entity::getPitch() const { return m_pitch; }

void Entity::setMoveIntent(const Vec3 &direction) { m_moveIntent = direction; }

void Entity::queueJump() { m_jumpQueued = true; }

void Entity::setNoGravity(bool value) { m_noGravity = value; }

void Entity::setNoCollision(bool value) { m_noCollision = value; }

void Entity::setFlying(bool value) {
    m_flying = value;
    if (m_flying) {
        m_velocity    = Vec3(0.0, 0.0, 0.0);
        m_noGravity   = true;
        m_noCollision = true;
    } else {
        m_noGravity   = false;
        m_noCollision = false;
    }
}

bool Entity::hasNoGravity() const { return m_noGravity; }

bool Entity::hasNoCollision() const { return m_noCollision; }

const AABB &Entity::getBoundingBox() const { return m_boundingBox; }

const AABB &Entity::getAABB() const { return m_aabb; }

void Entity::updateVectors() {
    float yawRad   = m_yaw * (float) (M_PI / 180.0);
    float pitchRad = m_pitch * (float) (M_PI / 180.0);

    Vec3 front(cos(yawRad) * cos(pitchRad), sin(pitchRad), sin(yawRad) * cos(pitchRad));
    m_front = front.normalize();
    m_right = m_front.cross(Vec3(0.0, 1.0, 0.0)).normalize();
    m_up    = m_right.cross(m_front).normalize();
}

void Entity::updateAABB() { m_aabb = m_boundingBox.translated(m_position); }

void Entity::applyPhysics(double deltaTime) {
    m_onGround = false;

    Vec3 wishDirection(0.0, 0.0, 0.0);
    if (m_moveIntent.lengthSquared() > 0.0) wishDirection = m_moveIntent.normalize();

    if (m_flying) m_velocity = wishDirection.scale(m_maxSpeed);
    else {
        m_velocity.x += wishDirection.x * m_acceleration * deltaTime;
        m_velocity.z += wishDirection.z * m_acceleration * deltaTime;

        m_velocity.x -= m_velocity.x * m_friction * deltaTime;
        m_velocity.z -= m_velocity.z * m_friction * deltaTime;

        double horizontalSpeed = sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
        if (horizontalSpeed > m_maxSpeed) {
            double scale = m_maxSpeed / horizontalSpeed;
            m_velocity.x *= scale;
            m_velocity.z *= scale;
        }
    }

    if (!m_noGravity) m_velocity.y += m_gravity * deltaTime;

    Vec3 movement = m_velocity.scale(deltaTime);

    if (m_noCollision || !m_world) {
        m_position = m_position.add(movement);
        return;
    }

    const double maxStep = 1.0 / 16.0;

    double remainingY = movement.y;
    while (remainingY != 0.0) {
        double step = remainingY;
        if (fabs(step) > maxStep) step = step > 0.0 ? maxStep : -maxStep;

        AABB test = m_aabb.translated(Vec3(0.0, step, 0.0));
        if (m_world->intersectsBlock(test)) {
            if (step < 0.0) m_onGround = true;

            m_velocity.y = 0.0;
            remainingY   = 0.0;
            break;
        }

        m_position.y += step;
        updateAABB();
        remainingY -= step;
    }

    if (m_onGround && m_velocity.y < 0.0) m_velocity.y = 0.0;

    if (m_jumpQueued && m_onGround) {
        m_velocity.y = m_jumpVelocity;
        m_onGround   = false;
    }

    double remainingX = movement.x;
    while (remainingX != 0.0) {
        double step = remainingX;
        if (fabs(step) > maxStep) step = step > 0.0 ? maxStep : -maxStep;

        AABB test = m_aabb.translated(Vec3(step, 0.0, 0.0));
        if (m_world->intersectsBlock(test)) {
            m_velocity.x = 0.0;
            remainingX   = 0.0;
            break;
        }

        m_position.x += step;
        updateAABB();
        remainingX -= step;
    }

    double remainingZ = movement.z;
    while (remainingZ != 0.0) {
        double step = remainingZ;
        if (fabs(step) > maxStep) step = step > 0.0 ? maxStep : -maxStep;

        AABB test = m_aabb.translated(Vec3(0.0, 0.0, step));
        if (m_world->intersectsBlock(test)) {
            m_velocity.z = 0.0;
            remainingZ   = 0.0;
            break;
        }

        m_position.z += step;
        updateAABB();
        remainingZ -= step;
    }
}
