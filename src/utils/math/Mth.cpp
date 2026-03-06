#include "Mth.h"

#include <cmath>

int Mth::floorDiv(int a, int b)
{
    int r = a / b;
    if ((a ^ b) < 0 && a % b)
    {
        r--;
    }
    return r;
}

int Mth::floorMod(int a, int b)
{
    int m = a % b;
    if (m < 0)
    {
        m += b;
    }
    return m;
}

float Mth::radiansToDegrees(float radians) { return radians * (180.0f / (float) M_PI); }

double Mth::clamp(double value, double min, double max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

float Mth::clampf(float value, float min, float max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

double Mth::lerp(double a, double b, double t) { return a + (b - a) * t; }

float Mth::lerpf(float a, float b, float t) { return a + (b - a) * t; }
