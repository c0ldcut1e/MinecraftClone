#pragma once

class Time {
public:
    static void update();
    static double delta();

private:
    static double s_delta;
    static double s_last;
};
