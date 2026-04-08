#include "Level.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

#include "../core/Minecraft.h"
#include "../entity/Entity.h"
#include "../entity/TestEntity.h"
#include "../utils/Time.h"
#include "../utils/math/Mth.h"
#include "LevelRenderer.h"
#include "block/Block.h"
#include "lighting/LightEngine.h"
#include "lighting/dynamic/DynamicLightManager.h"
#include "particle/ParticleEngine.h"
#include "particle/ParticleSpawnParams.h"
#include "render/LevelRenderObjectManager.h"

static uint8_t encodeDirection(Direction *direction)
{
    if (direction == Direction::NORTH)
    {
        return 1;
    }
    if (direction == Direction::SOUTH)
    {
        return 2;
    }
    if (direction == Direction::WEST)
    {
        return 3;
    }
    if (direction == Direction::EAST)
    {
        return 4;
    }
    if (direction == Direction::DOWN)
    {
        return 5;
    }
    if (direction == Direction::UP)
    {
        return 6;
    }
    return 0;
}

static Direction *oppositeDirection(Direction *direction)
{
    if (direction == Direction::NORTH)
    {
        return Direction::SOUTH;
    }
    if (direction == Direction::SOUTH)
    {
        return Direction::NORTH;
    }
    if (direction == Direction::WEST)
    {
        return Direction::EAST;
    }
    if (direction == Direction::EAST)
    {
        return Direction::WEST;
    }
    if (direction == Direction::DOWN)
    {
        return Direction::UP;
    }
    if (direction == Direction::UP)
    {
        return Direction::DOWN;
    }
    return nullptr;
}

static Direction *decodeDirection(uint8_t face)
{
    if (face == 1)
    {
        return Direction::NORTH;
    }
    if (face == 2)
    {
        return Direction::SOUTH;
    }
    if (face == 3)
    {
        return Direction::WEST;
    }
    if (face == 4)
    {
        return Direction::EAST;
    }
    if (face == 5)
    {
        return Direction::DOWN;
    }
    if (face == 6)
    {
        return Direction::UP;
    }
    return nullptr;
}

Level::Level()
    : m_dimension(), m_entities(), m_scheduledBlockTicks(), m_worldBorderEnabled(false),
      m_worldBorderChunks(32)
{
    m_dimension.setEmptyChunksSolid(false);
    m_particleEngine      = std::make_unique<ParticleEngine>();
    m_renderObjectManager = std::make_unique<LevelRenderObjectManager>();
    m_dynamicLightManager = std::make_unique<DynamicLightManager>();
}

Level::~Level() {}

void Level::update(float partialTicks)
{
    updateChunks();
    updateLighting();
    updateMeshes();
    updateParticles();

    Minecraft::getInstance()->getLocalPlayer()->update(partialTicks);
}

void Level::tick()
{
    processScheduledBlockTicks(m_dimension.getDimensionTime().getTicks());
    tickEntities();
    m_dimension.advanceTime();
}

void Level::updateChunks()
{
    if (ChunkManager *chunkManager = Minecraft::getInstance()->getChunkManager())
    {
        std::deque<std::pair<ChunkPos, std::unique_ptr<Chunk>>> ready;
        chunkManager->drainFinished(&ready, 2);

        const int chunkIntakeBudget = 2;
        int intook                  = 0;

        while (!ready.empty() && intook < chunkIntakeBudget)
        {
            intook++;
            auto [pos, chunkPtr] = std::move(ready.front());
            ready.pop_front();

            if (!m_dimension.adoptChunk(pos, std::move(chunkPtr)))
            {
                continue;
            }

            BlockPos blockPos(pos.x * Chunk::SIZE_X, pos.y * Chunk::SIZE_Y, pos.z * Chunk::SIZE_Z);
            m_dimension.markChunkDirty(blockPos);
            if (hasChunk(ChunkPos(pos.x - 1, pos.y, pos.z)))
            {
                m_dimension.markChunkDirty(
                        BlockPos(blockPos.x - Chunk::SIZE_X, blockPos.y, blockPos.z));
            }
            if (hasChunk(ChunkPos(pos.x + 1, pos.y, pos.z)))
            {
                m_dimension.markChunkDirty(
                        BlockPos(blockPos.x + Chunk::SIZE_X, blockPos.y, blockPos.z));
            }
            if (hasChunk(ChunkPos(pos.x, pos.y, pos.z - 1)))
            {
                m_dimension.markChunkDirty(
                        BlockPos(blockPos.x, blockPos.y, blockPos.z - Chunk::SIZE_Z));
            }
            if (hasChunk(ChunkPos(pos.x, pos.y, pos.z + 1)))
            {
                m_dimension.markChunkDirty(
                        BlockPos(blockPos.x, blockPos.y, blockPos.z + Chunk::SIZE_Z));
            }
        }
    }

    const int urgentBudget = 2;

    LevelRenderer *levelRenderer = Minecraft::getInstance()->getLevelRenderer();
    for (int i = 0; i < urgentBudget; i++)
    {
        ChunkPos pos;
        if (!m_dimension.pollUrgentDirtyChunk(&pos))
        {
            break;
        }

        if (levelRenderer)
        {
            levelRenderer->rebuildChunkUrgent(pos);
        }
    }
}

void Level::updateLighting()
{
    const int lightBudget = 32;

    for (int processed = 0; processed < lightBudget; processed++)
    {
        BlockPos pos;
        if (!m_dimension.pollLightUpdate(&pos))
        {
            break;
        }

        LightEngine::updateFrom(this, pos);
    }
}

void Level::updateMeshes()
{
    const int meshBudget = 16;

    for (int i = 0; i < meshBudget; i++)
    {
        ChunkPos pos;
        if (!m_dimension.pollDirtyChunk(&pos))
        {
            break;
        }

        Minecraft::getInstance()->getLevelRenderer()->rebuildChunk(pos);
    }
}

void Level::updateParticles()
{
    if (m_particleEngine)
    {
        m_particleEngine->tick(Time::getDelta());
    }
}

Dimension *Level::getDimension() { return &m_dimension; }

const Dimension *Level::getDimension() const { return &m_dimension; }

Chunk *Level::getChunk(const ChunkPos &pos) { return m_dimension.getChunk(pos); }

const Chunk *Level::getChunk(const ChunkPos &pos) const { return m_dimension.getChunk(pos); }

Chunk &Level::createChunk(const ChunkPos &pos) { return m_dimension.createChunk(pos); }

bool Level::hasChunk(const ChunkPos &pos) const { return m_dimension.hasChunk(pos); }

const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &Level::getChunks() const
{
    return m_dimension.getChunks();
}

void Level::markChunkDirty(const BlockPos &pos) { m_dimension.markChunkDirty(pos); }

void Level::markChunkDirtyUrgent(const ChunkPos &pos) { m_dimension.markChunkDirtyUrgent(pos); }

void Level::clearDirtyChunks() { m_dimension.clearDirtyChunks(); }

const std::deque<ChunkPos> &Level::getDirtyChunks() const { return m_dimension.getDirtyChunks(); }

size_t Level::getQueuedDirtyChunkCount() const { return m_dimension.getQueuedDirtyChunkCount(); }

size_t Level::getUrgentDirtyChunkCount() const { return m_dimension.getUrgentDirtyChunkCount(); }

void Level::addEntity(std::unique_ptr<Entity> entity) { m_entities.push_back(std::move(entity)); }

void Level::removeEntity(Entity *entity)
{
    m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
                                    [entity](const std::unique_ptr<Entity> &ptr) {
                                        return ptr.get() == entity;
                                    }),
                     m_entities.end());
}

void Level::tickEntities()
{
    for (std::unique_ptr<Entity> &entity : m_entities)
    {
        entity->storeOld();
        entity->tick();

        if (entity->getType() == TestEntity::TYPE)
        {
            entity->setMoveIntent(Vec3(0.0, 0.0, 0.5));
            Vec3 pos = entity->getPosition();
            if (entity->getLevel()->getBlockId(BlockPos((int) floor(pos.x), (int) floor(pos.y),
                                                        (int) floor(pos.z + 1.0))) != 0)
            {
                entity->queueJump();
            }
        }
    }
}

const std::vector<std::unique_ptr<Entity>> &Level::getEntities() const { return m_entities; }

ParticleEngine *Level::getParticleEngine() const { return m_particleEngine.get(); }

void Level::spawnParticle(ParticleType type, const Vec3 &position)
{
    spawnParticle(type, position, Vec3(), 1);
}

void Level::spawnParticle(ParticleType type, const Vec3 &position, int amount)
{
    spawnParticle(type, position, Vec3(), amount);
}

void Level::spawnParticle(ParticleType type, const Vec3 &position, const Vec3 &spread, int amount)
{
    ParticleSpawnParams params;
    params.type     = type;
    params.position = position;
    params.spread   = spread;
    params.amount   = amount;
    spawnParticle(params);
}

void Level::spawnParticle(const ParticleSpawnParams &params)
{
    if (m_particleEngine)
    {
        m_particleEngine->spawn(params);
    }
}

uint64_t Level::addRenderObject(const LevelRenderObject &object)
{
    if (!m_renderObjectManager)
    {
        return 0;
    }

    return m_renderObjectManager->add(object);
}

uint64_t Level::addChunkRenderObject(const ChunkPos &chunkPos, const LevelRenderObject &object)
{
    if (!m_renderObjectManager)
    {
        return 0;
    }

    return m_renderObjectManager->addChunkObject(chunkPos, object);
}

bool Level::updateRenderObject(const LevelRenderObject &object)
{
    if (!m_renderObjectManager)
    {
        return false;
    }

    return m_renderObjectManager->update(object);
}

bool Level::removeRenderObject(uint64_t id)
{
    if (!m_renderObjectManager)
    {
        return false;
    }

    return m_renderObjectManager->remove(id);
}

void Level::clearRenderObjects()
{
    if (m_renderObjectManager)
    {
        m_renderObjectManager->clear();
    }
}

void Level::copyRenderObjects(std::vector<LevelRenderObject> *out) const
{
    if (!m_renderObjectManager)
    {
        out->clear();
        return;
    }

    m_renderObjectManager->copy(out);
}

uint64_t Level::addDirectionalDynamicLight(const DirectionalDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->add(light);
}

uint64_t Level::addPointDynamicLight(const PointDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->add(light);
}

uint64_t Level::addAreaDynamicLight(const AreaDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->add(light);
}

uint64_t Level::addChunkDirectionalDynamicLight(const ChunkPos &chunkPos,
                                                const DirectionalDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->addChunkLight(chunkPos, light);
}

uint64_t Level::addChunkPointDynamicLight(const ChunkPos &chunkPos, const PointDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->addChunkLight(chunkPos, light);
}

uint64_t Level::addChunkAreaDynamicLight(const ChunkPos &chunkPos, const AreaDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return 0;
    }

    return m_dynamicLightManager->addChunkLight(chunkPos, light);
}

bool Level::updateDirectionalDynamicLight(const DirectionalDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->update(light);
}

bool Level::updatePointDynamicLight(const PointDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->update(light);
}

bool Level::updateAreaDynamicLight(const AreaDynamicLight &light)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->update(light);
}

bool Level::removeDirectionalDynamicLight(uint64_t id)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->removeDirectional(id);
}

bool Level::removePointDynamicLight(uint64_t id)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->removePoint(id);
}

bool Level::removeAreaDynamicLight(uint64_t id)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->removeArea(id);
}

bool Level::removeDynamicLight(uint64_t id)
{
    if (!m_dynamicLightManager)
    {
        return false;
    }

    return m_dynamicLightManager->remove(id);
}

void Level::clearDynamicLights()
{
    if (m_dynamicLightManager)
    {
        m_dynamicLightManager->clear();
    }
}

void Level::clearDirectionalDynamicLights()
{
    if (m_dynamicLightManager)
    {
        m_dynamicLightManager->clearDirectional();
    }
}

void Level::clearPointDynamicLights()
{
    if (m_dynamicLightManager)
    {
        m_dynamicLightManager->clearPoint();
    }
}

void Level::clearAreaDynamicLights()
{
    if (m_dynamicLightManager)
    {
        m_dynamicLightManager->clearArea();
    }
}

void Level::copyDirectionalDynamicLights(std::vector<DirectionalDynamicLight> *out) const
{
    if (!m_dynamicLightManager)
    {
        out->clear();
        return;
    }

    m_dynamicLightManager->copy(out);
}

void Level::copyPointDynamicLights(std::vector<PointDynamicLight> *out) const
{
    if (!m_dynamicLightManager)
    {
        out->clear();
        return;
    }

    m_dynamicLightManager->copy(out);
}

void Level::copyAreaDynamicLights(std::vector<AreaDynamicLight> *out) const
{
    if (!m_dynamicLightManager)
    {
        out->clear();
        return;
    }

    m_dynamicLightManager->copy(out);
}

uint32_t Level::getBlockId(const BlockPos &pos) const { return m_dimension.getBlockId(pos); }

void Level::setBlock(const BlockPos &pos, Block *block) { setBlock(pos, block, nullptr); }

void Level::setBlock(const BlockPos &pos, Block *block, Direction *placedAgainst)
{
    if (pos.y == 0 && block == Block::byName("air"))
    {
        return;
    }

    int cx = Mth::floorDiv(pos.x, Chunk::SIZE_X);
    int cy = Mth::floorDiv(pos.y, Chunk::SIZE_Y);
    int cz = Mth::floorDiv(pos.z, Chunk::SIZE_Z);

    int lx = Mth::floorMod(pos.x, Chunk::SIZE_X);
    int ly = Mth::floorMod(pos.y, Chunk::SIZE_Y);
    int lz = Mth::floorMod(pos.z, Chunk::SIZE_Z);

    ChunkPos chunkPos{cx, cy, cz};
    Chunk *chunk = getChunk(chunkPos);
    if (!chunk)
    {
        return;
    }

    Block *oldBlock = Block::byId(chunk->getBlockId(lx, ly, lz));
    if (oldBlock && oldBlock != block)
    {
        oldBlock->onBreak(this, pos);
    }

    chunk->setBlock(lx, ly, lz, block);

    Direction *supportFace = oppositeDirection(placedAgainst);
    chunk->setBlockAttachmentFace(lx, ly, lz, encodeDirection(supportFace));

    if (block && oldBlock != block)
    {
        block->onPlace(this, pos);
    }

    m_dimension.markChunkDirtyUrgent(chunkPos);
    if (lx == 0)
    {
        ChunkPos neighborPos(cx - 1, cy, cz);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    if (lx == Chunk::SIZE_X - 1)
    {
        ChunkPos neighborPos(cx + 1, cy, cz);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    if (ly == 0)
    {
        ChunkPos neighborPos(cx, cy - 1, cz);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    if (ly == Chunk::SIZE_Y - 1)
    {
        ChunkPos neighborPos(cx, cy + 1, cz);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    if (lz == 0)
    {
        ChunkPos neighborPos(cx, cy, cz - 1);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    if (lz == Chunk::SIZE_Z - 1)
    {
        ChunkPos neighborPos(cx, cy, cz + 1);
        if (hasChunk(neighborPos))
        {
            m_dimension.markChunkDirtyUrgent(neighborPos);
        }
    }
    m_dimension.queueLightUpdate(pos);
}

void Level::scheduleBlockForTick(const BlockPos &pos, uint32_t delayTicks, int priority)
{
    m_scheduledBlockTicks.push(
            ScheduledBlockTick{m_dimension.getDimensionTime().getTicks() + (uint64_t) delayTicks,
                               priority, pos, Block::byId(getBlockId(pos))});
}

void Level::setWorldBorderEnabled(bool enabled) { m_worldBorderEnabled = enabled; }

bool Level::isWorldBorderEnabled() const { return m_worldBorderEnabled; }

void Level::setWorldBorderChunks(int chunkRadius)
{
    if (chunkRadius < 0)
    {
        chunkRadius = 0;
    }

    m_worldBorderChunks = chunkRadius;
}

int Level::getWorldBorderChunks() const { return m_worldBorderChunks; }

bool Level::isChunkInsideWorldBorder(const ChunkPos &pos) const
{
    if (!m_worldBorderEnabled)
    {
        return true;
    }

    return abs(pos.x) <= m_worldBorderChunks && abs(pos.z) <= m_worldBorderChunks;
}

void Level::setEmptyChunksSolid(bool solid) { m_dimension.setEmptyChunksSolid(solid); }

bool Level::areEmptyChunksSolid() const { return m_dimension.areEmptyChunksSolid(); }

int Level::getSurfaceHeight(int levelX, int levelZ) const
{
    return m_dimension.getSurfaceHeight(levelX, levelZ);
}

bool Level::intersectsBlock(const AABB &aabb) const { return m_dimension.intersectsBlock(aabb); }

HitResult *Level::clip(const Vec3 &origin, const Vec3 &direction, float maxDistance)
{
    HitResult *result = new HitResult();
    result->setMiss();

    Vec3 normalizedDirection = direction.normalize();
    if (normalizedDirection.lengthSquared() == 0.0f)
    {
        return result;
    }

    float bestEntityT = std::numeric_limits<float>::max();
    UUID bestEntityUUID;
    uint64_t bestEntityType = 0;
    bool hasEntity          = false;
    UUID localUUID          = Minecraft::getInstance()->getLocalPlayer()->getUUID();

    for (const std::unique_ptr<Entity> &entity : m_entities)
    {
        if (entity->getUUID() == localUUID)
        {
            continue;
        }

        const AABB &box = entity->getAABB();
        const Vec3 &min = box.getMin();
        const Vec3 &max = box.getMax();

        float tMin = 0.0f;
        float tMax = maxDistance;
        bool valid = true;

        for (int axis = 0; axis < 3 && valid; axis++)
        {
            double o    = axis == 0 ? origin.x : (axis == 1 ? origin.y : origin.z);
            double d    = axis == 0 ? normalizedDirection.x
                                    : (axis == 1 ? normalizedDirection.y : normalizedDirection.z);
            double bMin = axis == 0 ? min.x : (axis == 1 ? min.y : min.z);
            double bMax = axis == 0 ? max.x : (axis == 1 ? max.y : max.z);

            if (d != 0.0)
            {
                float inv = (float) (1.0 / d);
                float t0  = (float) ((bMin - o) * inv);
                float t1  = (float) ((bMax - o) * inv);
                if (t0 > t1)
                {
                    std::swap(t0, t1);
                }
                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);
                if (tMax < tMin)
                {
                    valid = false;
                }
            }
            else if (o < bMin || o > bMax)
            {
                valid = false;
            }
        }

        if (valid && tMin >= 0.0f && tMin < bestEntityT)
        {
            bestEntityT    = tMin;
            bestEntityUUID = entity->getUUID();
            bestEntityType = entity->getType();
            hasEntity      = true;
        }
    }

    BlockPos pos((int) floor(origin.x), (int) floor(origin.y), (int) floor(origin.z));

    int stepX = normalizedDirection.x > 0.0f ? 1 : -1;
    int stepY = normalizedDirection.y > 0.0f ? 1 : -1;
    int stepZ = normalizedDirection.z > 0.0f ? 1 : -1;

    float tDeltaX = normalizedDirection.x != 0.0f ? (float) fabs(1.0 / normalizedDirection.x)
                                                  : std::numeric_limits<float>::max();
    float tDeltaY = normalizedDirection.y != 0.0f ? (float) fabs(1.0 / normalizedDirection.y)
                                                  : std::numeric_limits<float>::max();
    float tDeltaZ = normalizedDirection.z != 0.0f ? (float) fabs(1.0 / normalizedDirection.z)
                                                  : std::numeric_limits<float>::max();

    double nextBoundaryX = stepX > 0 ? (double) pos.x + 1.0 : (double) pos.x;
    double nextBoundaryY = stepY > 0 ? (double) pos.y + 1.0 : (double) pos.y;
    double nextBoundaryZ = stepZ > 0 ? (double) pos.z + 1.0 : (double) pos.z;

    float tMaxX = normalizedDirection.x != 0.0f
                          ? (float) ((nextBoundaryX - origin.x) / normalizedDirection.x)
                          : std::numeric_limits<float>::max();
    float tMaxY = normalizedDirection.y != 0.0f
                          ? (float) ((nextBoundaryY - origin.y) / normalizedDirection.y)
                          : std::numeric_limits<float>::max();
    float tMaxZ = normalizedDirection.z != 0.0f
                          ? (float) ((nextBoundaryZ - origin.z) / normalizedDirection.z)
                          : std::numeric_limits<float>::max();

    if (tMaxX < 0.0f)
    {
        tMaxX = 0.0f;
    }
    if (tMaxY < 0.0f)
    {
        tMaxY = 0.0f;
    }
    if (tMaxZ < 0.0f)
    {
        tMaxZ = 0.0f;
    }

    float bestBlockT = std::numeric_limits<float>::max();
    BlockPos bestBlockPos;
    Direction *bestFace = nullptr;
    bool hasBlock       = false;
    float t             = 0.0f;

    while (t <= maxDistance)
    {
        Direction *face = nullptr;

        if (tMaxX < tMaxY)
        {
            if (tMaxX < tMaxZ)
            {
                pos.x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                face = stepX > 0 ? Direction::WEST : Direction::EAST;
            }
            else
            {
                pos.z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? Direction::NORTH : Direction::SOUTH;
            }
        }
        else
        {
            if (tMaxY < tMaxZ)
            {
                pos.y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                face = stepY > 0 ? Direction::DOWN : Direction::UP;
            }
            else
            {
                pos.z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                face = stepZ > 0 ? Direction::NORTH : Direction::SOUTH;
            }
        }

        if (t > maxDistance)
        {
            break;
        }

        ChunkPos chunkPos(Mth::floorDiv(pos.x, Chunk::SIZE_X), Mth::floorDiv(pos.y, Chunk::SIZE_Y),
                          Mth::floorDiv(pos.z, Chunk::SIZE_Z));

        int lx = Mth::floorMod(pos.x, Chunk::SIZE_X);
        int lz = Mth::floorMod(pos.z, Chunk::SIZE_Z);

        Chunk *chunk = getChunk(chunkPos);
        if (!chunk)
        {
            continue;
        }

        if (pos.y < 0 || pos.y >= Chunk::SIZE_Y)
        {
            continue;
        }

        Block *block = Block::byId((int) chunk->getBlockId(lx, pos.y, lz));
        if (!block)
        {
            continue;
        }
        if (!block->isSelectable())
        {
            continue;
        }

        AABB blockBox = block->getPlacedAABB(
                pos, decodeDirection(chunk->getBlockAttachmentFace(lx, pos.y, lz)));
        const Vec3 &min = blockBox.getMin();
        const Vec3 &max = blockBox.getMax();

        if (max.x <= min.x || max.y <= min.y || max.z <= min.z)
        {
            continue;
        }

        float hitT         = 0.0f;
        float hitTMax      = maxDistance;
        bool valid         = true;
        Direction *hitFace = nullptr;

        for (int axis = 0; axis < 3 && valid; axis++)
        {
            double axisOrigin = axis == 0 ? origin.x : (axis == 1 ? origin.y : origin.z);
            double axisDirection =
                    axis == 0 ? normalizedDirection.x
                              : (axis == 1 ? normalizedDirection.y : normalizedDirection.z);
            double axisMin = axis == 0 ? min.x : (axis == 1 ? min.y : min.z);
            double axisMax = axis == 0 ? max.x : (axis == 1 ? max.y : max.z);

            Direction *axisNearFace =
                    axis == 0 ? Direction::WEST : (axis == 1 ? Direction::DOWN : Direction::NORTH);
            Direction *axisFarFace =
                    axis == 0 ? Direction::EAST : (axis == 1 ? Direction::UP : Direction::SOUTH);

            if (axisDirection != 0.0)
            {
                float inverseAxisDirection = (float) (1.0 / axisDirection);
                float entryT = (float) ((axisMin - axisOrigin) * inverseAxisDirection);
                float exitT  = (float) ((axisMax - axisOrigin) * inverseAxisDirection);
                if (entryT > exitT)
                {
                    std::swap(entryT, exitT);
                    std::swap(axisNearFace, axisFarFace);
                }

                if (entryT > hitT)
                {
                    hitT    = entryT;
                    hitFace = axisNearFace;
                }

                hitTMax = std::min(hitTMax, exitT);
                if (hitTMax < hitT)
                {
                    valid = false;
                }
            }
            else if (axisOrigin < axisMin || axisOrigin > axisMax)
            {
                valid = false;
            }
        }

        float finalHitT = hitT >= 0.0f ? hitT : hitTMax;
        if (!valid || finalHitT < 0.0f || finalHitT > maxDistance)
        {
            continue;
        }
        if (finalHitT + 1e-4f < t)
        {
            continue;
        }

        bestBlockT   = finalHitT;
        bestBlockPos = pos;
        bestFace     = hitFace ? hitFace : face;
        hasBlock     = true;
        break;
    }

    if (hasEntity && (!hasBlock || bestEntityT <= bestBlockT))
    {
        Vec3 hitPos = origin.add(normalizedDirection.scale(bestEntityT));
        result->setEntity(hitPos, bestEntityUUID, bestEntityType, bestEntityT);
        return result;
    }

    if (hasBlock)
    {
        Vec3 hitPos = origin.add(normalizedDirection.scale(bestBlockT));
        result->setBlock(hitPos, bestBlockPos, bestFace, bestBlockT);
        return result;
    }

    return result;
}

void Level::setRenderDistance(int distance) { m_dimension.setRenderDistance(distance); }

int Level::getRenderDistance() const { return m_dimension.getRenderDistance(); }

uint8_t Level::getLightLevel(const BlockPos &pos) const
{
    uint8_t sky   = getSkyLightLevel(pos);
    uint8_t block = getBlockLightLevel(pos);
    return std::max(sky, block);
}

uint8_t Level::getSkyLightLevel(const BlockPos &pos) const
{
    return LightEngine::getSkyLight((Level *) this, pos);
}

uint8_t Level::getBlockLightLevel(const BlockPos &pos) const
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    LightEngine::getBlockLight((Level *) this, pos, &r, &g, &b);
    return std::max(r, std::max(g, b));
}

void Level::queueLightUpdate(const BlockPos &pos) { m_dimension.queueLightUpdate(pos); }

size_t Level::getQueuedLightUpdateCount() const { return m_dimension.getQueuedLightUpdateCount(); }

Fog &Level::getFog() { return m_dimension.getFog(); }

const Fog &Level::getFog() const { return m_dimension.getFog(); }

const DimensionTime &Level::getDimensionTime() const { return m_dimension.getDimensionTime(); }

void Level::setDimensionTime(uint64_t ticks) { m_dimension.setDimensionTime(ticks); }

float Level::getDayFraction() const { return m_dimension.getDayFraction(); }

float Level::getCelestialAngle() const { return m_dimension.getCelestialAngle(); }

float Level::getDaylightFactor() const { return m_dimension.getDaylightFactor(); }

uint8_t Level::getSkyLightClamp() const { return m_dimension.getSkyLightClamp(); }

void Level::setDaylightCurveTicks(int darkStartTick, int darkPeakTick)
{
    m_dimension.setDaylightCurveTicks(darkStartTick, darkPeakTick);
}

int Level::getDarkStartTick() const { return m_dimension.getDarkStartTick(); }

int Level::getDarkPeakTick() const { return m_dimension.getDarkPeakTick(); }

void Level::processScheduledBlockTicks(uint64_t nowTick)
{
    static constexpr int maxPerTick = 4096;
    int processed                   = 0;

    while (!m_scheduledBlockTicks.empty() && processed < maxPerTick)
    {
        const ScheduledBlockTick top = m_scheduledBlockTicks.top();
        if (top.dueTick > nowTick)
        {
            break;
        }

        m_scheduledBlockTicks.pop();
        processed++;

        if (Block::byId(getBlockId(top.pos)) != top.block)
        {
            continue;
        }

        top.block->tick(this, top.pos);
    }
}
