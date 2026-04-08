#pragma once

class DimensionTime;

class LightSource
{
public:
    static float sampleSkyLightClamp(const DimensionTime &dimensionTime);
};
