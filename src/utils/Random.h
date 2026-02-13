#pragma once

#include <cstdint>

class Random {
public:
    explicit Random(uint32_t seed);

    uint32_t nextUInt();
    int nextInt(int min, int max);
    float nextFloat();
    double nextDouble();

private:
    uint32_t m_state;
};
