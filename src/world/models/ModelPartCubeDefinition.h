#pragma once

#include <string>
#include <vector>

#include "../../utils/math/Vec3.h"

class ModelPartCubeDefinition {
public:
    struct UVRect {
        int x;
        int y;
        int width;
        int height;
    };

    struct FaceUV {
        UVRect left;
        UVRect right;
        UVRect front;
        UVRect back;
        UVRect top;
        UVRect bottom;
    };

    ModelPartCubeDefinition();
    ModelPartCubeDefinition(const std::string &name, const Vec3 &min, const Vec3 &max, const FaceUV &uv, float inflate);

    const std::string &getName() const;
    const Vec3 &getMin() const;
    const Vec3 &getMax() const;
    const FaceUV &getUV() const;
    float getInflate() const;

private:
    std::string m_name;
    Vec3 m_min;
    Vec3 m_max;
    FaceUV m_uv;
    float m_inflate;
};
