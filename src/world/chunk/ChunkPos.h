#pragma once

#include <cstddef>
#include <functional>

struct ChunkPos {
    ChunkPos() : x(0), y(0), z(0) {}
    ChunkPos(int x, int y, int z) : x(x), y(y), z(z) {}

    bool operator==(const ChunkPos &other) const { return x == other.x && y == other.y && z == other.z; }

    bool operator!=(const ChunkPos &other) const { return !(*this == other); }

    ChunkPos operator+(const ChunkPos &other) const { return {x + other.x, y + other.y, z + other.z}; }

    ChunkPos operator-(const ChunkPos &other) const { return {x - other.x, y - other.y, z - other.z}; }

    ChunkPos &operator+=(const ChunkPos &other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    ChunkPos &operator-=(const ChunkPos &other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    bool operator<(const ChunkPos &other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }

    int x;
    int y;
    int z;
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos &pos) const {
        size_t h1 = std::hash<int>()(pos.x);
        size_t h2 = std::hash<int>()(pos.y);
        size_t h3 = std::hash<int>()(pos.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
