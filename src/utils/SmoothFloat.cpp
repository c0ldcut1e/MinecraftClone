#include "SmoothFloat.h"

#include "math/Mth.h"

SmoothFloat::SmoothFloat() : m_value(0.0f), m_hasValue(false) {}

SmoothFloat::SmoothFloat(float value) : m_value(value), m_hasValue(true) {}

float SmoothFloat::update(float target, float amount)
{
    amount = Mth::clampf(amount, 0.0f, 1.0f);
    if (!m_hasValue)
    {
        m_value    = target;
        m_hasValue = true;
        return m_value;
    }

    m_value = Mth::lerpf(m_value, target, amount);
    return m_value;
}

void SmoothFloat::reset(float value)
{
    m_value    = value;
    m_hasValue = true;
}

float SmoothFloat::getValue() const { return m_value; }
