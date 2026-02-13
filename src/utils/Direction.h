#pragma once

#include <string>

class Direction {
public:
    Direction(const char *name, int dx, int dy, int dz);

    const std::string name;
    int dx;
    int dy;
    int dz;

    static Direction *NORTH;
    static Direction *SOUTH;
    static Direction *EAST;
    static Direction *WEST;
    static Direction *UP;
    static Direction *DOWN;
};
