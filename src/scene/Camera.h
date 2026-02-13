#pragma once

#include "../utils/math/Mat4.h"
#include "../utils/math/Vec3.h"

class Camera {
public:
    Camera();

    void setPosition(const Vec3 &position);
    void setDirection(const Vec3 &front, const Vec3 &up);

    Mat4 getViewMatrix() const;

private:
    Vec3 m_position;
    Vec3 m_front;
    Vec3 m_up;
};
