#include "Memory.h"

#include <cstring>
#include <new>

#include "MemoryTracker.h"

void *Memory::allocate(size_t bytes)
{
    if (bytes == 0)
    {
        return nullptr;
    }

    MemoryTracker::getInstance()->trackAllocation(bytes);
    return ::operator new(bytes);
}

void Memory::deallocate(void *ptr, size_t bytes)
{
    if (!ptr)
    {
        return;
    }

    if (bytes > 0)
    {
        MemoryTracker::getInstance()->trackFree(bytes);
    }

    ::operator delete(ptr);
}

void Memory::copy(void *dst, const void *src, size_t bytes)
{
    if (!dst || !src || bytes == 0)
    {
        return;
    }

    memcpy(dst, src, bytes);
}

void Memory::set(void *dst, int value, size_t bytes)
{
    if (!dst || bytes == 0)
    {
        return;
    }

    memset(dst, value, bytes);
}
