#include "Math.h"

#include <cmath>

int Math::floorDiv(int a, int b) {
    int r = a / b;
    if ((a ^ b) < 0 && a % b) r--;
    return r;
}

int Math::floorMod(int a, int b) {
    int m = a % b;
    if (m < 0) m += b;
    return m;
}

float Math::radiansToDegrees(float radians) { return radians * (180.0f / (float) M_PI); }
