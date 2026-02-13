#pragma once

#include "LivingEntity.h"

class Player : public LivingEntity {
public:
    explicit Player(World *world);

    uint64_t getType() override;
    void tick() override;

    void setMoveForward(bool value);
    void setMoveBackward(bool value);
    void setMoveLeft(bool value);
    void setMoveRight(bool value);

    void setFlying(bool value);
    bool isFlying() const;

    static constexpr uint64_t TYPE = 0x1000000000000003;

protected:
    bool m_moveForward;
    bool m_moveBackward;
    bool m_moveLeft;
    bool m_moveRight;
};
