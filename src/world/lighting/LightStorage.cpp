#include "LightStorage.h"

#include <cmath>

LightStorage::LightStorage() : m_skyLightClamp(15.0f), m_initialized(false) {}

void LightStorage::reset(float skyLightClamp)
{
    m_skyLightClamp = skyLightClamp;
    m_initialized   = true;
}

void LightStorage::update(float targetSkyLightClamp, double delta, bool smooth)
{
    if (!m_initialized)
    {
        reset(targetSkyLightClamp);
        return;
    }

    if (!smooth)
    {
        m_skyLightClamp = targetSkyLightClamp;
        return;
    }

    float blend = 1.0f - expf(-(float) delta * 20.0f);
    if (blend < 0.0f)
    {
        blend = 0.0f;
    }
    if (blend > 1.0f)
    {
        blend = 1.0f;
    }

    m_skyLightClamp += (targetSkyLightClamp - m_skyLightClamp) * blend;

    if (fabsf(targetSkyLightClamp - m_skyLightClamp) < 0.0001f)
    {
        m_skyLightClamp = targetSkyLightClamp;
    }
}

float LightStorage::getSkyLightClamp() const { return m_skyLightClamp; }
