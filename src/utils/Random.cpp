#include "Random.h"

Random::Random(uint32_t seed) : m_state(seed ? seed : 0xDEADBEEF) {}

uint32_t Random::nextUInt() {
    uint32_t x = m_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_state = x;
    return x;
}

int Random::nextInt(int min, int max) {
    uint32_t value = nextUInt();
    return min + (int) (value % (uint32_t) (max - min + 1));
}

float Random::nextFloat() { return (float) (nextUInt() / (double) UINT32_MAX); }

double Random::nextDouble() { return (double) (nextUInt() / (double) UINT32_MAX); }
