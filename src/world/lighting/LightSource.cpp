#include "LightSource.h"

#include "../DimensionTime.h"

float LightSource::sampleSkyLightClamp(const DimensionTime &dimensionTime)
{
    int tickInDay     = (int) (dimensionTime.getTicks() % (uint64_t) DimensionTime::DAY_TICKS);
    int darkStartTick = dimensionTime.getDarkStartTick();
    int darkPeakTick  = dimensionTime.getDarkPeakTick();

    int duskLength = darkPeakTick - darkStartTick;
    if (duskLength < 0)
    {
        duskLength += DimensionTime::DAY_TICKS;
    }
    if (duskLength < 1)
    {
        return 15.0f;
    }

    int dayPeakTick = (darkPeakTick + 12000) % DimensionTime::DAY_TICKS;
    int dawnStartTick =
            ((dayPeakTick - duskLength) % DimensionTime::DAY_TICKS + DimensionTime::DAY_TICKS) %
            DimensionTime::DAY_TICKS;
    int nightLength =
            ((dawnStartTick - darkPeakTick) % DimensionTime::DAY_TICKS + DimensionTime::DAY_TICKS) %
            DimensionTime::DAY_TICKS;

    int fromDusk =
            ((tickInDay - darkStartTick) % DimensionTime::DAY_TICKS + DimensionTime::DAY_TICKS) %
            DimensionTime::DAY_TICKS;
    int fromNight =
            ((tickInDay - darkPeakTick) % DimensionTime::DAY_TICKS + DimensionTime::DAY_TICKS) %
            DimensionTime::DAY_TICKS;
    int fromDawn =
            ((tickInDay - dawnStartTick) % DimensionTime::DAY_TICKS + DimensionTime::DAY_TICKS) %
            DimensionTime::DAY_TICKS;

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
