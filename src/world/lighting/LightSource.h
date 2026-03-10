#pragma once

class WorldTime;

class LightSource
{
public:
    static float sampleSkyLightClamp(const WorldTime &worldTime);
};
