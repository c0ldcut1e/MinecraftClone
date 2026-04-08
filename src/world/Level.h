#pragma once

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "../utils/HitResult.h"
#include "Dimension.h"
#include "lighting/dynamic/DynamicLight.h"
#include "render/LevelRenderObject.h"

class Entity;
class ParticleEngine;
class LevelRenderObjectManager;
class DynamicLightManager;
struct ParticleSpawnParams;
enum class ParticleType : uint16_t;

class Level
{
public:
    Level();
    ~Level();

    void update(float partialTicks);
    void tick();

    void updateChunks();
    void updateLighting();
    void updateMeshes();
    void updateParticles();

    Dimension *getDimension();
    const Dimension *getDimension() const;

    Chunk *getChunk(const ChunkPos &pos);
    const Chunk *getChunk(const ChunkPos &pos) const;
    Chunk &createChunk(const ChunkPos &pos);
    bool hasChunk(const ChunkPos &pos) const;
    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &getChunks() const;
    void markChunkDirty(const BlockPos &pos);
    void markChunkDirtyUrgent(const ChunkPos &pos);
    void clearDirtyChunks();
    const std::deque<ChunkPos> &getDirtyChunks() const;
    size_t getQueuedDirtyChunkCount() const;
    size_t getUrgentDirtyChunkCount() const;

    void addEntity(std::unique_ptr<Entity> entity);
    void removeEntity(Entity *entity);
    void tickEntities();
    const std::vector<std::unique_ptr<Entity>> &getEntities() const;
    ParticleEngine *getParticleEngine() const;
    void spawnParticle(ParticleType type, const Vec3 &position);
    void spawnParticle(ParticleType type, const Vec3 &position, int amount);
    void spawnParticle(ParticleType type, const Vec3 &position, const Vec3 &spread, int amount);
    void spawnParticle(const ParticleSpawnParams &params);
    uint64_t addRenderObject(const LevelRenderObject &object);
    uint64_t addChunkRenderObject(const ChunkPos &chunkPos, const LevelRenderObject &object);
    bool updateRenderObject(const LevelRenderObject &object);
    bool removeRenderObject(uint64_t id);
    void clearRenderObjects();
    void copyRenderObjects(std::vector<LevelRenderObject> *out) const;
    uint64_t addDirectionalDynamicLight(const DirectionalDynamicLight &light);
    uint64_t addPointDynamicLight(const PointDynamicLight &light);
    uint64_t addAreaDynamicLight(const AreaDynamicLight &light);
    uint64_t addChunkDirectionalDynamicLight(const ChunkPos &chunkPos,
                                             const DirectionalDynamicLight &light);
    uint64_t addChunkPointDynamicLight(const ChunkPos &chunkPos, const PointDynamicLight &light);
    uint64_t addChunkAreaDynamicLight(const ChunkPos &chunkPos, const AreaDynamicLight &light);
    bool updateDirectionalDynamicLight(const DirectionalDynamicLight &light);
    bool updatePointDynamicLight(const PointDynamicLight &light);
    bool updateAreaDynamicLight(const AreaDynamicLight &light);
    bool removeDirectionalDynamicLight(uint64_t id);
    bool removePointDynamicLight(uint64_t id);
    bool removeAreaDynamicLight(uint64_t id);
    bool removeDynamicLight(uint64_t id);
    void clearDynamicLights();
    void clearDirectionalDynamicLights();
    void clearPointDynamicLights();
    void clearAreaDynamicLights();
    void copyDirectionalDynamicLights(std::vector<DirectionalDynamicLight> *out) const;
    void copyPointDynamicLights(std::vector<PointDynamicLight> *out) const;
    void copyAreaDynamicLights(std::vector<AreaDynamicLight> *out) const;

    uint32_t getBlockId(const BlockPos &pos) const;
    void setBlock(const BlockPos &pos, Block *block);
    void setBlock(const BlockPos &pos, Block *block, Direction *placedAgainst);
    void scheduleBlockForTick(const BlockPos &pos, uint32_t delayTicks, int priority);
    void setWorldBorderEnabled(bool enabled);
    bool isWorldBorderEnabled() const;
    void setWorldBorderChunks(int chunkRadius);
    int getWorldBorderChunks() const;
    bool isChunkInsideWorldBorder(const ChunkPos &pos) const;
    void setEmptyChunksSolid(bool solid);
    bool areEmptyChunksSolid() const;

    int getSurfaceHeight(int levelX, int levelZ) const;
    bool intersectsBlock(const AABB &aabb) const;
    HitResult *clip(const Vec3 &origin, const Vec3 &direction, float maxDistance);

    void setRenderDistance(int distance);
    int getRenderDistance() const;

    uint8_t getLightLevel(const BlockPos &pos) const;
    uint8_t getSkyLightLevel(const BlockPos &pos) const;
    uint8_t getBlockLightLevel(const BlockPos &pos) const;
    void queueLightUpdate(const BlockPos &pos);
    size_t getQueuedLightUpdateCount() const;

    Fog &getFog();
    const Fog &getFog() const;

    const DimensionTime &getDimensionTime() const;
    void setDimensionTime(uint64_t ticks);

    float getDayFraction() const;
    float getCelestialAngle() const;
    float getDaylightFactor() const;
    uint8_t getSkyLightClamp() const;

    void setDaylightCurveTicks(int darkStartTick, int darkPeakTick);
    int getDarkStartTick() const;
    int getDarkPeakTick() const;

private:
    struct ScheduledBlockTick
    {
        bool operator<(const ScheduledBlockTick &other) const
        {
            if (dueTick != other.dueTick)
            {
                return dueTick > other.dueTick;
            }
            return priority > other.priority;
        }

        uint64_t dueTick;
        int priority;
        BlockPos pos;
        Block *block;
    };

    void processScheduledBlockTicks(uint64_t nowTick);

    Dimension m_dimension;
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::unique_ptr<ParticleEngine> m_particleEngine;
    std::unique_ptr<LevelRenderObjectManager> m_renderObjectManager;
    std::unique_ptr<DynamicLightManager> m_dynamicLightManager;
    std::priority_queue<ScheduledBlockTick> m_scheduledBlockTicks;
    bool m_worldBorderEnabled;
    int m_worldBorderChunks;
};
