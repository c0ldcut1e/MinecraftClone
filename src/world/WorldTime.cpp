#include "WorldTime.h"

#include <cmath>

WorldTime::WorldTime()
    : m_ticks(DEFAULT_START_TICKS), m_darkStartTick(DEFAULT_DARK_START_TICK), m_darkPeakTick(DEFAULT_DARK_PEAK_TICK), m_dayFraction(0.0f), m_celestialAngle(0.0f), m_daylightFactor(1.0f), m_sunDirection(1.0f, 0.0f, 0.0f), m_moonDirection(-1.0f, 0.0f, 0.0f),
      m_skyLightClamp(15) {
    recompute();
}

void WorldTime::advance() {
    m_ticks++;
    recompute();
}

void WorldTime::setTicks(uint64_t ticks) {
    m_ticks = ticks;
    recompute();
}

uint64_t WorldTime::getTicks() const { return m_ticks; }

float WorldTime::getDayFraction() const { return m_dayFraction; }

float WorldTime::getCelestialAngle() const { return m_celestialAngle; }

float WorldTime::getDaylightFactor() const { return m_daylightFactor; }

Vec3 WorldTime::getSunDirection() const { return m_sunDirection; }

Vec3 WorldTime::getMoonDirection() const { return m_moonDirection; }

uint8_t WorldTime::getSkyLightClamp() const { return m_skyLightClamp; }

int WorldTime::getDarkStartTick() const { return m_darkStartTick; }

int WorldTime::getDarkPeakTick() const { return m_darkPeakTick; }

void WorldTime::setDaylightCurve(int darkStartTick, int darkPeakTick) {
    darkStartTick = ((darkStartTick % DAY_TICKS) + DAY_TICKS) % DAY_TICKS;
    darkPeakTick  = ((darkPeakTick % DAY_TICKS) + DAY_TICKS) % DAY_TICKS;

    m_darkStartTick = darkStartTick;
    m_darkPeakTick  = darkPeakTick;

    recompute();
}

void WorldTime::recompute() {
    const int tickInDay = (int) (m_ticks % (uint64_t) DAY_TICKS);

    m_dayFraction = (float) tickInDay / (float) DAY_TICKS;

    {
        float shifted = m_dayFraction - 0.25f;
        if (shifted < 0.0f) shifted += 1.0f;

        float eased      = 1.0f - (float) ((cos((double) shifted * M_PI) + 1.0) * 0.5);
        m_celestialAngle = shifted + (eased - shifted) / 3.0f;
    }

    {
        int duskLength = m_darkPeakTick - m_darkStartTick;
        if (duskLength < 0) duskLength += DAY_TICKS;

        if (duskLength < 1) {
            m_daylightFactor = 1.0f;
        } else {
            const int dayPeakTick   = (m_darkPeakTick + 12000) % DAY_TICKS;
            const int dawnStartTick = ((dayPeakTick - duskLength) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int nightLength   = ((dawnStartTick - m_darkPeakTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;

            const int fromDusk  = ((tickInDay - m_darkStartTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int fromNight = ((tickInDay - m_darkPeakTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int fromDawn  = ((tickInDay - dawnStartTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;

            if (fromDusk < duskLength) {
                float progress   = (float) fromDusk / (float) duskLength;
                m_daylightFactor = 0.5f + 0.5f * cosf(progress * (float) M_PI);
            } else if (fromNight < nightLength)
                m_daylightFactor = 0.0f;
            else if (fromDawn < duskLength) {
                float progress   = (float) fromDawn / (float) duskLength;
                m_daylightFactor = 0.5f - 0.5f * cosf(progress * (float) M_PI);
            } else
                m_daylightFactor = 1.0f;
        }
    }

    {
        const float celestialRadians = m_celestialAngle * 6.28318530718f;
        m_sunDirection               = Vec3((double) cosf(celestialRadians), (double) sinf(celestialRadians), 0.0);
        m_moonDirection              = Vec3(-(double) cosf(celestialRadians), -(double) sinf(celestialRadians), 0.0);
    }

    {
        int duskLength = m_darkPeakTick - m_darkStartTick;
        if (duskLength < 0) duskLength += DAY_TICKS;
        if (duskLength < 1) m_skyLightClamp = 15;
        else {
            const int dayPeakTick   = (m_darkPeakTick + 12000) % DAY_TICKS;
            const int dawnStartTick = ((dayPeakTick - duskLength) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int nightLength   = ((dawnStartTick - m_darkPeakTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;

            const int fromDusk  = ((tickInDay - m_darkStartTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int fromNight = ((tickInDay - m_darkPeakTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;
            const int fromDawn  = ((tickInDay - dawnStartTick) % DAY_TICKS + DAY_TICKS) % DAY_TICKS;

            int clamp;
            if (fromDusk < duskLength) {
                clamp = 15 - (fromDusk * 15) / duskLength;
                if (clamp < 0) clamp = 0;
            } else if (fromNight < nightLength)
                clamp = 0;
            else if (fromDawn < duskLength) {
                clamp = (fromDawn * 15) / duskLength;
                if (clamp > 15) clamp = 15;
            } else
                clamp = 15;

            m_skyLightClamp = (uint8_t) clamp;
        }
    }
}
