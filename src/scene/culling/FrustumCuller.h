#pragma once

#include "../../utils/math/Mat4.h"
#include "Culler.h"
#include "FrustumData.h"

class FrustumCuller : public Culler
{
public:
    FrustumCuller();

    void extractPlanes(const Mat4 &viewProjection);

    const FrustumData &getData() const;

    bool testAABB(const AABB &aabb) const override;

private:
    FrustumData m_data;
};
