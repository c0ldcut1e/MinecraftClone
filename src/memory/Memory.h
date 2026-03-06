#pragma once

#include <cstddef>

class Memory
{
public:
    static void *allocate(size_t bytes);
    static void deallocate(void *ptr, size_t bytes = 0);

    static void copy(void *dst, const void *src, size_t bytes);
    static void set(void *dst, int value, size_t bytes);
};
