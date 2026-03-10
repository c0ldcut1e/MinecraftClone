#include "LightSource.h"

#include "../WorldTime.h"

float LightSource::sampleSkyLightClamp(const WorldTime &worldTime)
{
    int tickInDay = (int) (worldTime.getTicks() % (uint64_t) WorldTime::DAY_TICKS);
    int darkStartTick = worldTime.getDarkStartTick();
    int darkPeakTick  = worldTime.getDarkPeakTick();

    int duskLength = darkPeakTick - darkStartTick;
    if (duskLength < 0)
    {
        duskLength += WorldTime::DAY_TICKS;
    }
    if (duskLength < 1)
    {
        return 15.0f;
    }

    int dayPeakTick = (darkPeakTick + 12000) % WorldTime::DAY_TICKS;
    int dawnStartTick =
            ((dayPeakTick - duskLength) % WorldTime::DAY_TICKS + WorldTime::DAY_TICKS) %
            WorldTime::DAY_TICKS;
    int nightLength =
            ((dawnStartTick - darkPeakTick) % WorldTime::DAY_TICKS + WorldTime::DAY_TICKS) %
            WorldTime::DAY_TICKS;

    int fromDusk =
            ((tickInDay - darkStartTick) % WorldTime::DAY_TICKS + WorldTime::DAY_TICKS) %
            WorldTime::DAY_TICKS;
    int fromNight =
            ((tickInDay - darkPeakTick) % WorldTime::DAY_TICKS + WorldTime::DAY_TICKS) %
            WorldTime::DAY_TICKS;
    int fromDawn =
            ((tickInDay - dawnStartTick) % WorldTime::DAY_TICKS + WorldTime::DAY_TICKS) %
            WorldTime::DAY_TICKS;

    if (fromDusk < duskLength)
    {
        return 15.0f - (float) fromDusk * 15.0f / (float) duskLength;
    }
    if (fromNight < nightLength)
    {
        return 0.0f;
    }
    if (fromDawn < duskLength)
    {
        return (float) fromDawn * 15.0f / (float) duskLength;
    }
    return 15.0f;
}
