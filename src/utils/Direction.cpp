#include "Direction.h"

Direction::Direction(const char *name, int dx, int dy, int dz) : name(name), dx(dx), dy(dy), dz(dz) {}

Direction *Direction::NORTH = new Direction("north", 0, 0, -1);
Direction *Direction::SOUTH = new Direction("south", 0, 0, 1);
Direction *Direction::EAST  = new Direction("east", 1, 0, 0);
Direction *Direction::WEST  = new Direction("west", -1, 0, 0);
Direction *Direction::UP    = new Direction("up", 0, 1, 0);
Direction *Direction::DOWN  = new Direction("down", 0, -1, 0);
