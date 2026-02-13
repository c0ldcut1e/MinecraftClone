#pragma once

#include "Vec3.h"

class AABB {
public:
    AABB();
    AABB(const Vec3 &min, const Vec3 &max);

    const Vec3 &getMin() const;
    const Vec3 &getMax() const;

    void set(const Vec3 &min, const Vec3 &max);

    AABB translated(const Vec3 &offset) const;

    bool intersects(const AABB &other) const;

private:
    Vec3 m_min;
    Vec3 m_max;
};
