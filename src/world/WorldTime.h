#pragma once

#include <cstdint>

#include "../utils/math/Vec3.h"

class WorldTime {
public:
    static constexpr int DAY_TICKS                = 24000;
    static constexpr int DEFAULT_DARK_START_TICK  = 10000;
    static constexpr int DEFAULT_DARK_PEAK_TICK   = 15000;
    static constexpr uint64_t DEFAULT_START_TICKS = 0;

    WorldTime();

    void advance();
    void setTicks(uint64_t ticks);
    uint64_t getTicks() const;

    float getDayFraction() const;
    float getCelestialAngle() const;
    float getDaylightFactor() const;
    Vec3 getSunDirection() const;
    Vec3 getMoonDirection() const;
    uint8_t getSkyLightClamp() const;

    void setDaylightCurve(int darkStartTick, int darkPeakTick);
    int getDarkStartTick() const;
    int getDarkPeakTick() const;

private:
    void recompute();

    uint64_t m_ticks;
    int m_darkStartTick;
    int m_darkPeakTick;

    float m_dayFraction;
    float m_celestialAngle;
    float m_daylightFactor;
    Vec3 m_sunDirection;
    Vec3 m_moonDirection;
    uint8_t m_skyLightClamp;
};
