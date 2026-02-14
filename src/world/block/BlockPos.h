#pragma once

#include <cstddef>
#include <functional>

struct BlockPos {
    BlockPos() : x(0), y(0), z(0) {}
    BlockPos(int x, int y, int z) : x(x), y(y), z(z) {}

    bool operator==(const BlockPos &other) const { return x == other.x && y == other.y && z == other.z; }

    bool operator!=(const BlockPos &other) const { return !(*this == other); }

    BlockPos operator+(const BlockPos &other) const { return {x + other.x, y + other.y, z + other.z}; }

    BlockPos operator-(const BlockPos &other) const { return {x - other.x, y - other.y, z - other.z}; }

    BlockPos &operator+=(const BlockPos &other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    BlockPos &operator-=(const BlockPos &other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    int x;
    int y;
    int z;
};

namespace std {
    template<>
    struct hash<BlockPos> {
        size_t operator()(const BlockPos &pos) const noexcept {
            size_t h = pos.x;
            h ^= (size_t) pos.y << 1;
            h ^= (size_t) pos.z << 2;
            return h;
        }
    };
} // namespace std
