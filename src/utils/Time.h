#pragma once

class Time {
public:
    static void update();
    static double getDelta();

    static double getTickDelta();
    static void setTickDelta(double deltaTime);

private:
    static double s_delta;
    static double s_last;
    static double s_tickDelta;
};
