#pragma once

#include "../../utils/AABB.h"

class Culler
{
public:
    virtual ~Culler() = default;

    virtual bool testAABB(const AABB &aabb) const = 0;
};
