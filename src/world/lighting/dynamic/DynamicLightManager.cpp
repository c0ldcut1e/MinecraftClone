#include "DynamicLightManager.h"

uint64_t DynamicLightManager::add(const DirectionalDynamicLight &light)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    DirectionalDynamicLight copy = light;
    copy.id                      = m_nextId++;
    m_directionalLights[copy.id] = copy;
    return copy.id;
}

uint64_t DynamicLightManager::add(const PointDynamicLight &light)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    PointDynamicLight copy = light;
    copy.id                = m_nextId++;
    m_pointLights[copy.id] = copy;
    return copy.id;
}

uint64_t DynamicLightManager::add(const AreaDynamicLight &light)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    AreaDynamicLight copy = light;
    copy.id               = m_nextId++;
    m_areaLights[copy.id] = copy;
    return copy.id;
}

uint64_t DynamicLightManager::addChunkLight(const ChunkPos &chunkPos,
                                            const DirectionalDynamicLight &light)
{
    DirectionalDynamicLight copy = light;
    copy.chunkPos                = chunkPos;
    copy.renderInChunk           = true;
    return add(copy);
}

uint64_t DynamicLightManager::addChunkLight(const ChunkPos &chunkPos,
                                            const PointDynamicLight &light)
{
    PointDynamicLight copy = light;
    copy.chunkPos          = chunkPos;
    copy.renderInChunk     = true;
    return add(copy);
}

uint64_t DynamicLightManager::addChunkLight(const ChunkPos &chunkPos, const AreaDynamicLight &light)
{
    AreaDynamicLight copy = light;
    copy.chunkPos         = chunkPos;
    copy.renderInChunk    = true;
    return add(copy);
}

bool DynamicLightManager::update(const DirectionalDynamicLight &light)
{
    if (light.id == 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_directionalLights.find(light.id);
    if (it == m_directionalLights.end())
    {
        return false;
    }

    it->second = light;
    return true;
}

bool DynamicLightManager::update(const PointDynamicLight &light)
{
    if (light.id == 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pointLights.find(light.id);
    if (it == m_pointLights.end())
    {
        return false;
    }

    it->second = light;
    return true;
}

bool DynamicLightManager::update(const AreaDynamicLight &light)
{
    if (light.id == 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_areaLights.find(light.id);
    if (it == m_areaLights.end())
    {
        return false;
    }

    it->second = light;
    return true;
}

bool DynamicLightManager::removeDirectional(uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_directionalLights.erase(id) > 0;
}

bool DynamicLightManager::removePoint(uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pointLights.erase(id) > 0;
}

bool DynamicLightManager::removeArea(uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_areaLights.erase(id) > 0;
}

bool DynamicLightManager::remove(uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_directionalLights.erase(id) > 0)
    {
        return true;
    }
    if (m_pointLights.erase(id) > 0)
    {
        return true;
    }
    return m_areaLights.erase(id) > 0;
}

void DynamicLightManager::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_directionalLights.clear();
    m_pointLights.clear();
    m_areaLights.clear();
}

void DynamicLightManager::clearDirectional()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_directionalLights.clear();
}

void DynamicLightManager::clearPoint()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pointLights.clear();
}

void DynamicLightManager::clearArea()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_areaLights.clear();
}

void DynamicLightManager::copy(std::vector<DirectionalDynamicLight> *out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out->clear();
    out->reserve(m_directionalLights.size());

    for (const auto &entry : m_directionalLights)
    {
        out->push_back(entry.second);
    }
}

void DynamicLightManager::copy(std::vector<PointDynamicLight> *out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out->clear();
    out->reserve(m_pointLights.size());

    for (const auto &entry : m_pointLights)
    {
        out->push_back(entry.second);
    }
}

void DynamicLightManager::copy(std::vector<AreaDynamicLight> *out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    out->clear();
    out->reserve(m_areaLights.size());

    for (const auto &entry : m_areaLights)
    {
        out->push_back(entry.second);
    }
}

size_t DynamicLightManager::getCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_directionalLights.size() + m_pointLights.size() + m_areaLights.size();
}
