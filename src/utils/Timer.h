#pragma once

#include "SmoothFloat.h"

class Timer
{
public:
    Timer(float ticksPerSecond, float maxFrameSeconds);

    void advanceTime(double deltaSeconds);

    int getElapsedTicks() const;
    float getPartialTicks() const;
    float getFrameSeconds() const;

private:
    double m_ticksPerSecond;
    double m_tickDelta;
    double m_maxFrameSeconds;
    double m_accumulator;
    float m_partialTicks;
    int m_elapsedTicks;
    SmoothFloat m_frameDelta;
};
