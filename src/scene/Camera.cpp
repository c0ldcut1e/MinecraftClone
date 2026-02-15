#include "Camera.h"

Camera::Camera() : m_position(0.0, 0.0, 0.0), m_front(0.0, 0.0, -1.0), m_up(0.0, 1.0, 0.0) {}

void Camera::setPosition(const Vec3 &position) { m_position = position; }

const Vec3 &Camera::getPosition() const { return m_position; }

void Camera::setDirection(const Vec3 &front, const Vec3 &up) {
    m_front = front;
    m_up    = up;
}

Mat4 Camera::getViewMatrix() const { return Mat4::lookAt(m_position, m_position.add(m_front), m_up); }
