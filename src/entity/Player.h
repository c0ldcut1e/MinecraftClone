#pragma once

#include "LivingEntity.h"

class Player : public LivingEntity
{
public:
    explicit Player(Level *level, const std::wstring &name);

    uint64_t getType() override;

    void tick() override;

    void setMoveInputs(float forward, float strafe);

    void setFlying(bool value);
    bool isFlying() const;

    static constexpr uint64_t TYPE = 0x3000000000000000;

protected:
    float m_moveForwardInput;
    float m_moveStrafeInput;
};
