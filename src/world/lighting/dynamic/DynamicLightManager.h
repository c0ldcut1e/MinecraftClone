#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "DynamicLight.h"

class DynamicLightManager
{
public:
    uint64_t add(const DirectionalDynamicLight &light);
    uint64_t add(const PointDynamicLight &light);
    uint64_t add(const AreaDynamicLight &light);
    uint64_t addChunkLight(const ChunkPos &chunkPos, const DirectionalDynamicLight &light);
    uint64_t addChunkLight(const ChunkPos &chunkPos, const PointDynamicLight &light);
    uint64_t addChunkLight(const ChunkPos &chunkPos, const AreaDynamicLight &light);
    bool update(const DirectionalDynamicLight &light);
    bool update(const PointDynamicLight &light);
    bool update(const AreaDynamicLight &light);
    bool removeDirectional(uint64_t id);
    bool removePoint(uint64_t id);
    bool removeArea(uint64_t id);
    bool remove(uint64_t id);
    void clear();
    void clearDirectional();
    void clearPoint();
    void clearArea();
    void copy(std::vector<DirectionalDynamicLight> *out) const;
    void copy(std::vector<PointDynamicLight> *out) const;
    void copy(std::vector<AreaDynamicLight> *out) const;
    size_t getCount() const;

private:
    std::atomic<uint64_t> m_nextId{1};
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, DirectionalDynamicLight> m_directionalLights;
    std::unordered_map<uint64_t, PointDynamicLight> m_pointLights;
    std::unordered_map<uint64_t, AreaDynamicLight> m_areaLights;
};
