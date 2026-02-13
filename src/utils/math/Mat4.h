#pragma once

#include "Vec3.h"

class Mat4 {
public:
    Mat4();

    static Mat4 identity();
    static Mat4 perspective(double fovRadians, double aspect, double nearPlane, double farPlane);
    static Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up);
    static Mat4 orthographic(double left, double right, double bottom, double top, double nearPlane, double farPlane);

    Mat4 multiply(const Mat4 &other) const;

    const double *data() const;

private:
    double m_data[16];
};
