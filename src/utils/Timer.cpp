#include "Timer.h"

#include "Time.h"
#include "math/Mth.h"

Timer::Timer(float ticksPerSecond, float maxFrameSeconds)
    : m_ticksPerSecond(ticksPerSecond),
      m_tickDelta(ticksPerSecond > 0.0f ? 1.0f / ticksPerSecond : 0.0f),
      m_maxFrameSeconds(maxFrameSeconds), m_accumulator(0.0), m_partialTicks(0.0f),
      m_elapsedTicks(0), m_frameDelta((float) m_tickDelta)
{
    Time::setTickDelta((float) m_tickDelta);
}

void Timer::advanceTime(double deltaSeconds)
{
    double frameSeconds = Mth::clamp(deltaSeconds, 0.0, m_maxFrameSeconds);
    m_frameDelta.update((float) frameSeconds, 0.1f);

    m_elapsedTicks = 0;
    if (m_tickDelta <= 0.0f)
    {
        m_partialTicks = 0.0f;
        return;
    }

    m_accumulator += frameSeconds;
    while (m_accumulator >= m_tickDelta && m_elapsedTicks < 10)
    {
        m_accumulator -= m_tickDelta;
        m_elapsedTicks++;
    }

    m_partialTicks = (float) (m_accumulator / m_tickDelta);
    m_partialTicks = Mth::clampf(m_partialTicks, 0.0f, 1.0f);
}

int Timer::getElapsedTicks() const { return m_elapsedTicks; }

float Timer::getPartialTicks() const { return m_partialTicks; }

float Timer::getFrameSeconds() const { return m_frameDelta.getValue(); }
