#include "LevelRenderObjectManager.h"

uint64_t LevelRenderObjectManager::add(const LevelRenderObject &object)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    LevelRenderObject copy = object;
    copy.id                = m_nextId++;
    m_objects[copy.id]     = copy;
    return copy.id;
}

uint64_t LevelRenderObjectManager::addChunkObject(const ChunkPos &chunkPos,
                                                  const LevelRenderObject &object)
{
    LevelRenderObject copy = object;
    copy.chunkPos          = chunkPos;
    copy.renderInChunk     = true;
    return add(copy);
}

bool LevelRenderObjectManager::update(const LevelRenderObject &object)
{
    if (object.id == 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_objects.find(object.id);
    if (it == m_objects.end())
    {
        return false;
    }

    it->second = object;
    return true;
}

bool LevelRenderObjectManager::remove(uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_objects.erase(id) > 0;
}

void LevelRenderObjectManager::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_objects.clear();
}

void LevelRenderObjectManager::copy(std::vector<LevelRenderObject> *out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out->clear();
    out->reserve(m_objects.size());

    for (const auto &entry : m_objects)
    {
        out->push_back(entry.second);
    }
}
