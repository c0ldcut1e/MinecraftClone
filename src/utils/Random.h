#pragma once

#include <cstdint>

class Random {
public:
    Random();
    explicit Random(uint64_t seed);

    uint32_t nextUInt();
    uint64_t nextUInt64();
    int nextInt(int min, int max);
    float nextFloat();
    double nextDouble();

private:
    uint64_t m_state;

    static uint64_t seedFromSystem();
};
