#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "LevelRenderObject.h"

class LevelRenderObjectManager
{
public:
    uint64_t add(const LevelRenderObject &object);
    uint64_t addChunkObject(const ChunkPos &chunkPos, const LevelRenderObject &object);
    bool update(const LevelRenderObject &object);
    bool remove(uint64_t id);
    void clear();
    void copy(std::vector<LevelRenderObject> *out) const;

private:
    std::atomic<uint64_t> m_nextId{1};
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, LevelRenderObject> m_objects;
};
