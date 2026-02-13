#include "Mat4.h"

#include <cmath>

Mat4::Mat4() {
    for (int i = 0; i < 16; i++) m_data[i] = 0.0;
}

Mat4 Mat4::identity() {
    Mat4 result;
    result.m_data[0]  = 1.0;
    result.m_data[5]  = 1.0;
    result.m_data[10] = 1.0;
    result.m_data[15] = 1.0;
    return result;
}

Mat4 Mat4::lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
    Vec3 forward    = center.sub(eye).normalize();
    Vec3 right      = forward.cross(up).normalize();
    Vec3 _up        = right.cross(forward);
    Mat4 view       = identity();
    view.m_data[0]  = right.x;
    view.m_data[1]  = _up.x;
    view.m_data[2]  = -forward.x;
    view.m_data[4]  = right.y;
    view.m_data[5]  = _up.y;
    view.m_data[6]  = -forward.y;
    view.m_data[8]  = right.z;
    view.m_data[9]  = _up.z;
    view.m_data[10] = -forward.z;
    view.m_data[12] = -right.dot(eye);
    view.m_data[13] = -_up.dot(eye);
    view.m_data[14] = forward.dot(eye);
    return view;
}

Mat4 Mat4::perspective(double fovRadians, double aspect, double nearPlane, double farPlane) {
    Mat4 result;
    double tanHalfFov = tan(fovRadians / 2.0);
    result.m_data[0]  = 1.0 / (aspect * tanHalfFov);
    result.m_data[5]  = 1.0 / tanHalfFov;
    result.m_data[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.m_data[11] = -1.0;
    result.m_data[14] = -(2.0 * farPlane * nearPlane) / (farPlane - nearPlane);
    return result;
}

Mat4 Mat4::orthographic(double left, double right, double bottom, double top, double nearPlane, double farPlane) {
    Mat4 result;
    result.m_data[0]  = 2.0 / (right - left);
    result.m_data[5]  = 2.0 / (top - bottom);
    result.m_data[10] = -2.0 / (farPlane - nearPlane);
    result.m_data[12] = -(right + left) / (right - left);
    result.m_data[13] = -(top + bottom) / (top - bottom);
    result.m_data[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.m_data[15] = 1.0;
    return result;
}

Mat4 Mat4::translation(double x, double y, double z) {
    Mat4 result       = identity();
    result.m_data[12] = x;
    result.m_data[13] = y;
    result.m_data[14] = z;
    return result;
}

Mat4 Mat4::scale(double x, double y, double z) {
    Mat4 result;
    result.m_data[0]  = x;
    result.m_data[5]  = y;
    result.m_data[10] = z;
    result.m_data[15] = 1.0;
    return result;
}

Mat4 Mat4::rotation(double angleRadians, double x, double y, double z) {
    double len = sqrt(x * x + y * y + z * z);
    if (len == 0.0) return identity();

    x /= len;
    y /= len;
    z /= len;

    double c = cos(angleRadians);
    double s = sin(angleRadians);
    double t = 1.0 - c;

    Mat4 result = identity();

    result.m_data[0] = t * x * x + c;
    result.m_data[1] = t * x * y + s * z;
    result.m_data[2] = t * x * z - s * y;

    result.m_data[4] = t * x * y - s * z;
    result.m_data[5] = t * y * y + c;
    result.m_data[6] = t * y * z + s * x;

    result.m_data[8]  = t * x * z + s * y;
    result.m_data[9]  = t * y * z - s * x;
    result.m_data[10] = t * z * z + c;

    return result;
}

Mat4 Mat4::multiply(const Mat4 &other) const {
    Mat4 result;

    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            result.m_data[col + row * 4] = m_data[0 + row * 4] * other.m_data[col + 0 * 4] + m_data[1 + row * 4] * other.m_data[col + 1 * 4] + m_data[2 + row * 4] * other.m_data[col + 2 * 4] + m_data[3 + row * 4] * other.m_data[col + 3 * 4];

    return result;
}

const double *Mat4::data() const { return m_data; }
