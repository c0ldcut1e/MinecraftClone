#include "Random.h"

#include <ctime>

static uint64_t splitmix64(uint64_t &state) {
    uint64_t z = (state += 0x9E3779B97f4A7C15);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EB;
    return z ^ (z >> 31);
}

uint64_t Random::seedFromSystem() {
    uint64_t timePart = (uint64_t) std::time(nullptr);
    uint64_t addrPart = (uint64_t) (uintptr_t) &timePart;
    uint64_t seed     = timePart ^ addrPart;
    return seed ? seed : 0xDEADDEADDEADDEAD;
}

Random::Random() : m_state(seedFromSystem()) { splitmix64(m_state); }

Random::Random(uint64_t seed) : m_state(seed ? seed : 0xDEADDEADDEADDEAD) { splitmix64(m_state); }

uint64_t Random::nextUInt64() { return splitmix64(m_state); }

uint32_t Random::nextUInt() { return (uint32_t) (nextUInt64() >> 32); }

int Random::nextInt(int min, int max) { return min + (int) (nextUInt() % (uint32_t) (max - min + 1)); }

float Random::nextFloat() { return (float) (nextUInt() / (double) UINT32_MAX); }

double Random::nextDouble() { return (double) (nextUInt() / (double) UINT32_MAX); }
