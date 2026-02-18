#pragma once

#include <string>

#include "../utils/UUID.h"
#include "../utils/math/AABB.h"
#include "../utils/math/Vec3.h"
#include "../world/World.h"
#include "../world/models/Model.h"

class Entity {
public:
    explicit Entity(World *world);
    virtual ~Entity();

    virtual uint64_t getType();

    virtual void update(float partialTicks);
    virtual void tick();

    World *getWorld() const;

    void setPosition(const Vec3 &position);
    const Vec3 &getPosition() const;
    const Vec3 &getOldPosition() const;
    Vec3 getRenderPosition(float partialTicks) const;

    const Vec3 &getFront() const;
    float getYaw() const;
    float getPitch() const;
    float getOldYaw() const;
    float getOldPitch() const;
    float getRenderYaw(float partialTicks) const;
    float getRenderPitch(float partialTicks) const;

    void storeOld();

    void setMoveIntent(const Vec3 &direction);
    void queueJump();
    void jump();

    void setNoGravity(bool value);
    void setNoCollision(bool value);
    void setFlying(bool value);

    bool hasNoGravity() const;
    bool hasNoCollision() const;

    const AABB &getBoundingBox() const;
    const AABB &getAABB() const;

    const UUID &getUUID() const;

    void setName(const std::wstring &name);
    const std::wstring &getName() const;

    void setModel(Model *model);
    Model *getModel() const;

    static constexpr uint64_t TYPE = 0x1000000000000000;

    void updateVectors();
    void updateAABB();
    void applyPhysics(double deltaTime);

    World *m_world;

    int m_totalTickCount;

    Vec3 m_position;
    Vec3 m_oldPosition;

    Vec3 m_velocity;
    Vec3 m_moveIntent;

    Vec3 m_front;
    Vec3 m_right;
    Vec3 m_up;

    float m_yaw;
    float m_pitch;
    float m_oldYaw;
    float m_oldPitch;

    float m_gravity;
    float m_jumpVelocity;
    float m_maxSpeed;
    float m_acceleration;
    float m_friction;

    bool m_onGround;
    bool m_jumpQueued;

    bool m_noGravity;
    bool m_noCollision;
    bool m_flying;

    AABB m_boundingBox;
    AABB m_aabb;

    UUID m_uuid;
    std::wstring m_name;

    Model *m_model;
};
