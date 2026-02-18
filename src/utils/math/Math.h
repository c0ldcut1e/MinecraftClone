#pragma once

class Math {
public:
    static int floorDiv(int a, int b);
    static int floorMod(int a, int b);
    static float radiansToDegrees(float radians);
    static double clamp(double value, double min, double max);
    static float clampf(float value, float min, float max);
};
