#pragma once

#include <array>

class LightCache
{
public:
    LightCache();

    void rebuild(float skyLightClamp);

    float getSkyLightClamp() const;
    const std::array<float, 16> &getSkyLightLevels() const;

private:
    float m_skyLightClamp;
    std::array<float, 16> m_skyLightLevels;
};
