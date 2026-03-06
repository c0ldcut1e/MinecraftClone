#pragma once

#include "../../utils/math/Vec3.h"

struct FrustumPlane
{
    Vec3 normal;
    double distance;
};

struct FrustumData
{
    static constexpr int PLANE_COUNT = 6;
    FrustumPlane planes[PLANE_COUNT];
};
