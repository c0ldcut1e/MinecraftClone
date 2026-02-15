#pragma once

#include "../utils/math/AABB.h"
#include "../utils/math/Mat4.h"
#include "../utils/math/Vec3.h"

class Frustum {
public:
    struct Plane {
        Vec3 normal;
        double distance;
    };

    Frustum();

    void extractPlanes(const Mat4 &viewProjection);

    bool testAABB(const AABB &aabb) const;

private:
    Plane m_planes[6];
};