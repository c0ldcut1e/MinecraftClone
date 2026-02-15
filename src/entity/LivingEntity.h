#pragma once

#include "Entity.h"

class LivingEntity : public Entity {
public:
    explicit LivingEntity(World *world);
    ~LivingEntity() override;

    uint64_t getType() override;

    static constexpr uint64_t TYPE = 0x2000000000000000;
};
