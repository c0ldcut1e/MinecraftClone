#include "MemoryTracker.h"

MemoryTracker *MemoryTracker::getInstance()
{
    static MemoryTracker instance;
    return &instance;
}

void MemoryTracker::genLists(int count)
{
    if (count > 0)
    {
        m_glListCount.fetch_add(count);
    }
}

void MemoryTracker::deleteLists(int count)
{
    if (count > 0)
    {
        m_glListCount.fetch_sub(count);
    }
}

void MemoryTracker::genTextures(int count)
{
    if (count > 0)
    {
        m_glTextureCount.fetch_add(count);
    }
}

void MemoryTracker::deleteTextures(int count)
{
    if (count > 0)
    {
        m_glTextureCount.fetch_sub(count);
    }
}

void MemoryTracker::genBuffers(int count)
{
    if (count > 0)
    {
        m_glBufferCount.fetch_add(count);
    }
}

void MemoryTracker::deleteBuffers(int count)
{
    if (count > 0)
    {
        m_glBufferCount.fetch_sub(count);
    }
}

void MemoryTracker::genVertexArrays(int count)
{
    if (count > 0)
    {
        m_glVertexArrayCount.fetch_add(count);
    }
}

void MemoryTracker::deleteVertexArrays(int count)
{
    if (count > 0)
    {
        m_glVertexArrayCount.fetch_sub(count);
    }
}

std::vector<uint8_t> MemoryTracker::createByteBuffer(size_t count)
{
    trackAllocation(count);
    return std::vector<uint8_t>(count);
}

std::vector<int32_t> MemoryTracker::createIntBuffer(size_t count)
{
    trackAllocation(count * sizeof(int32_t));
    return std::vector<int32_t>(count);
}

std::vector<float> MemoryTracker::createFloatBuffer(size_t count)
{
    trackAllocation(count * sizeof(float));
    return std::vector<float>(count);
}

void MemoryTracker::trackAllocation(size_t bytes)
{
    if (bytes > 0)
    {
        m_trackedAllocatedBytes.fetch_add((uint64_t) bytes);
    }
}

void MemoryTracker::trackFree(size_t bytes)
{
    if (bytes > 0)
    {
        m_trackedAllocatedBytes.fetch_sub((uint64_t) bytes);
    }
}

MemoryTracker::Stats MemoryTracker::getStats() const
{
    return Stats{m_glListCount.load(), m_glTextureCount.load(), m_glBufferCount.load(),
                 m_glVertexArrayCount.load(), m_trackedAllocatedBytes.load()};
}
