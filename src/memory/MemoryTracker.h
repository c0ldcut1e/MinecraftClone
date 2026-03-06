#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class MemoryTracker
{
public:
    struct Stats
    {
        int glListCount;
        int glTextureCount;
        int glBufferCount;
        int glVertexArrayCount;
        uint64_t trackedAllocatedBytes;
    };

    static MemoryTracker *getInstance();

    void genLists(int count);
    void deleteLists(int count);

    void genTextures(int count);
    void deleteTextures(int count);

    void genBuffers(int count);
    void deleteBuffers(int count);

    void genVertexArrays(int count);
    void deleteVertexArrays(int count);

    std::vector<uint8_t> createByteBuffer(size_t count);
    std::vector<int32_t> createIntBuffer(size_t count);
    std::vector<float> createFloatBuffer(size_t count);

    void trackAllocation(size_t bytes);
    void trackFree(size_t bytes);

    Stats getStats() const;

private:
    MemoryTracker() = default;

    std::atomic<int> m_glListCount{0};
    std::atomic<int> m_glTextureCount{0};
    std::atomic<int> m_glBufferCount{0};
    std::atomic<int> m_glVertexArrayCount{0};
    std::atomic<uint64_t> m_trackedAllocatedBytes{0};
};
