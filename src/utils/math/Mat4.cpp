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

    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            result.m_data[col * 4 + row] = m_data[0 * 4 + row] * other.m_data[col * 4 + 0] + m_data[1 * 4 + row] * other.m_data[col * 4 + 1] + m_data[2 * 4 + row] * other.m_data[col * 4 + 2] + m_data[3 * 4 + row] * other.m_data[col * 4 + 3];

    return result;
}

Mat4 Mat4::inverse() const {
    const double *m = m_data;
    double inv[16];

    inv[0]  = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8]  = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5]  = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9]  = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2]  = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6]  = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3]  = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7]  = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0) return Mat4::identity();

    det = 1.0 / det;

    Mat4 out;
    for (int i = 0; i < 16; i++) out.m_data[i] = inv[i] * det;
    return out;
}

const double *Mat4::data() const { return m_data; }
