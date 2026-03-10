#pragma once

class LightStorage
{
public:
    LightStorage();

    void reset(float skyLightClamp);
    void update(float targetSkyLightClamp, double delta, bool smooth);

    float getSkyLightClamp() const;

private:
    float m_skyLightClamp;
    bool m_initialized;
};
