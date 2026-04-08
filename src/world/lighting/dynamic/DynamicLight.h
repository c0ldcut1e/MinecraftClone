#pragma once

#include <cstdint>
#include <string>

#include "../../../utils/math/Vec3.h"
#include "../../chunk/ChunkPos.h"

struct DynamicLightBase
{
    uint64_t id = 0;
    std::string name;
    ChunkPos chunkPos;
    Vec3 color         = Vec3(1.0, 1.0, 1.0);
    bool enabled       = true;
    bool renderInChunk = false;
    float brightness   = 1.0f;
};

struct DirectionalDynamicLight : DynamicLightBase
{
    Vec3 direction = Vec3(0.0, -1.0, 0.0);
};

struct PointDynamicLight : DynamicLightBase
{
    Vec3 position;
    float radius = 15.0f;
};

struct AreaDynamicLight : DynamicLightBase
{
    Vec3 position;
    Vec3 orientation;
    float width    = 1.0f;
    float height   = 1.0f;
    float angle    = 45.0f;
    float distance = 15.0f;
};
