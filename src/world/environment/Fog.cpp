#include "Fog.h"

Fog::Fog() : m_enabled(true), m_disableInCaves(true), m_startFactor(0.7f), m_endFactor(0.9f), m_color(0.435294118, 0.709803922, 0.97254902), m_lutX(0.5f), m_lutY(0.5f) {}

bool Fog::isEnabled() const { return m_enabled; }

void Fog::setEnabled(bool enabled) { m_enabled = enabled; }

bool Fog::isDisabledInCaves() const { return m_disableInCaves; }

void Fog::setDisableInCaves(bool disable) { m_disableInCaves = disable; }

float Fog::getStartFactor() const { return m_startFactor; }

float Fog::getEndFactor() const { return m_endFactor; }

void Fog::setRangeFactors(float startFactor, float endFactor) {
    m_startFactor = startFactor;
    m_endFactor   = endFactor;
}

const Vec3 &Fog::getColor() const { return m_color; }

void Fog::setColor(const Vec3 &color) { m_color = color; }

void Fog::setColor(float r, float g, float b) { m_color = Vec3(r, g, b); }

float Fog::getLutX() const { return m_lutX; }

float Fog::getLutY() const { return m_lutY; }

void Fog::setLut(float x, float y) {
    m_lutX = x;
    m_lutY = y;
}
