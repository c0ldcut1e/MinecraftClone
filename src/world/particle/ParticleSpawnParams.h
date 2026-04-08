#pragma once

#include <cstdint>

#include "../../utils/math/Vec3.h"
#include "ParticleType.h"

struct ParticleSpawnParams
{
    ParticleSpawnParams() : position(), spread(), amount(1) {}

    ParticleType type;
    Vec3 position;
    Vec3 spread;
    int amount;
};
