#include "AABB.h"

AABB::AABB() : m_min(0.0, 0.0, 0.0), m_max(0.0, 0.0, 0.0) {}

AABB::AABB(const Vec3 &min, const Vec3 &max) : m_min(min), m_max(max) {}

const Vec3 &AABB::getMin() const { return m_min; }

const Vec3 &AABB::getMax() const { return m_max; }

void AABB::set(const Vec3 &min, const Vec3 &max) {
    m_min = min;
    m_max = max;
}

AABB AABB::translated(const Vec3 &offset) const { return AABB(m_min.add(offset), m_max.add(offset)); }

bool AABB::intersects(const AABB &other) const {
    if (m_max.x <= other.m_min.x || m_min.x >= other.m_max.x) return false;
    if (m_max.y <= other.m_min.y || m_min.y >= other.m_max.y) return false;
    if (m_max.z <= other.m_min.z || m_min.z >= other.m_max.z) return false;
    return true;
}
