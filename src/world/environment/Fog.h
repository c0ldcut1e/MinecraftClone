#pragma once

#include "../../utils/math/Vec3.h"

class Fog {
public:
    Fog();

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isDisabledInCaves() const;
    void setDisableInCaves(bool disable);

    float getStartFactor() const;
    float getEndFactor() const;
    void setRangeFactors(float startFactor, float endFactor);

    const Vec3 &getColor() const;
    void setColor(const Vec3 &color);
    void setColor(float r, float g, float b);

    float getLutX() const;
    float getLutY() const;
    void setLut(float x, float y);

private:
    bool m_enabled;
    bool m_disableInCaves;

    float m_startFactor;
    float m_endFactor;

    Vec3 m_color;

    float m_lutX;
    float m_lutY;
};
