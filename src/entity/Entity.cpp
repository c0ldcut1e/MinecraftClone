#include "Entity.h"

#include <cmath>

#include "../utils/Time.h"

Entity::Entity(World *world)
    : m_world(world), m_totalTickCount(-1), m_position(0.0, 0.0, 0.0), m_velocity(0.0, 0.0, 0.0), m_moveIntent(0.0, 0.0, 0.0), m_front(0.0, 0.0, -1.0), m_up(0.0, 1.0, 0.0), m_yaw(-90.0f), m_pitch(0.0f), m_gravity(-32.0f), m_jumpVelocity(9.6f),
      m_maxSpeed(20.0f), m_acceleration(200.0f), m_friction(12.5f), m_onGround(false), m_jumpQueued(false), m_noGravity(false), m_noCollision(false), m_flying(false), m_boundingBox(Vec3(-0.3, 0.0, -0.3), Vec3(0.3, 1.8, 0.3)), m_aabb(), m_uuid(UUID::random()),
      m_name(L""), m_model(nullptr) {}

Entity::~Entity() {}

uint64_t Entity::getType() { return TYPE; }

void Entity::update(float partialTicks) { (void) partialTicks; }

void Entity::tick() {
    m_totalTickCount++;

    updateVectors();
    updateAABB();

    applyPhysics(Time::getTickDelta());

    m_moveIntent = Vec3(0.0, 0.0, 0.0);
    m_jumpQueued = false;
}

World *Entity::getWorld() const { return m_world; }

void Entity::setPosition(const Vec3 &position) { m_position = position; }

const Vec3 &Entity::getPosition() const { return m_position; }

const Vec3 &Entity::getOldPosition() const { return m_oldPosition; }

Vec3 Entity::getRenderPosition(float partialTicks) const {
    const Vec3 &a = m_oldPosition;
    const Vec3 &b = m_position;
    return Vec3(a.x + (b.x - a.x) * partialTicks, a.y + (b.y - a.y) * partialTicks, a.z + (b.z - a.z) * partialTicks);
}

const Vec3 &Entity::getFront() const { return m_front; }

float Entity::getYaw() const { return m_yaw; }

float Entity::getPitch() const { return m_pitch; }

float Entity::getOldYaw() const { return m_oldYaw; }

float Entity::getOldPitch() const { return m_oldPitch; }

float Entity::getRenderYaw(float partialTicks) const {
    float d = m_yaw - m_oldYaw;
    while (d <= -180.0f) d += 360.0f;
    while (d > 180.0f) d -= 360.0f;
    return m_oldYaw + d * partialTicks;
}

float Entity::getRenderPitch(float partialTicks) const { return m_oldPitch + (m_pitch - m_oldPitch) * partialTicks; }

void Entity::storeOld() {
    m_oldPosition = m_position;
    m_oldYaw      = m_yaw;
    m_oldPitch    = m_pitch;
}

void Entity::setMoveIntent(const Vec3 &direction) { m_moveIntent = direction; }

void Entity::queueJump() { m_jumpQueued = true; }

void Entity::jump() {
    if (m_onGround) {
        m_velocity.y = m_jumpVelocity;
        m_onGround   = false;
    }
}

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
    wishDirection = m_moveIntent;

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

    if (m_jumpQueued) jump();

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

const UUID &Entity::getUUID() const { return m_uuid; }

void Entity::setName(const std::wstring &name) { m_name = name; }

const std::wstring &Entity::getName() const { return m_name; }

void Entity::setModel(Model *model) { m_model = model; }

Model *Entity::getModel() const { return m_model; }
