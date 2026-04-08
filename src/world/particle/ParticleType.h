#pragma once

#include <cstddef>
#include <cstdint>

enum class ParticleType : uint16_t
{
    SPLASH,
};

struct ParticleTypeHash
{
    size_t operator()(ParticleType type) const { return (size_t) type; }
};
