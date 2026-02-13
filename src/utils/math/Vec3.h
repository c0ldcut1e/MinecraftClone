#pragma once

class Vec3 {
public:
    Vec3();
    Vec3(double _x, double _y, double _z);

    Vec3 add(const Vec3 &other) const;
    Vec3 sub(const Vec3 &other) const;
    Vec3 scale(double scalar) const;

    double dot(const Vec3 &other) const;
    Vec3 cross(const Vec3 &other) const;

    Vec3 normalize() const;

    double length() const;
    double lengthSquared() const;

    double x;
    double y;
    double z;
};
