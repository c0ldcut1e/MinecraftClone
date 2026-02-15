#pragma once

#include "LivingEntity.h"

class TestEntity : public LivingEntity {
public:
    explicit TestEntity(World *world);
    ~TestEntity() override;

    uint64_t getType() override;
    void tick() override;

    float getLimbSwing() const;
    float getLimbSwingOld() const;
    float getLimbSwingAmount() const;
    float getLimbSwingAmountOld() const;

    static constexpr uint64_t TYPE = 0x2000000000000001;

private:
    bool m_hasLastPos;
    Vec3 m_lastPos;

    float m_limbSwing;
    float m_limbSwingAmount;
    float m_limbSwingOld;
    float m_limbSwingAmountOld;
};
