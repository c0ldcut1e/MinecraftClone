#pragma once

class SmoothFloat
{
public:
    SmoothFloat();
    explicit SmoothFloat(float value);

    float update(float target, float amount);
    void reset(float value);

    float getValue() const;

private:
    float m_value;
    bool m_hasValue;
};
