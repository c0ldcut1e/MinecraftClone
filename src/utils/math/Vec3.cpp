#include "Vec3.h"

#include <cmath>

Vec3::Vec3() : x(0.0), y(0.0), z(0.0) {}

Vec3::Vec3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}

Vec3 Vec3::add(const Vec3 &other) const { return Vec3(x + other.x, y + other.y, z + other.z); }

Vec3 Vec3::sub(const Vec3 &other) const { return Vec3(x - other.x, y - other.y, z - other.z); }

Vec3 Vec3::scale(double scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }

double Vec3::dot(const Vec3 &other) const { return x * other.x + y * other.y + z * other.z; }

Vec3 Vec3::cross(const Vec3 &other) const { return Vec3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x); }

Vec3 Vec3::normalize() const {
    double len = length();
    if (len == 0.0) return Vec3();
    return Vec3(x / len, y / len, z / len);
}

double Vec3::length() const { return sqrt(x * x + y * y + z * z); }

double Vec3::lengthSquared() const { return x * x + y * y + z * z; }
