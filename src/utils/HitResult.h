#pragma once

#include "../utils/UUID.h"
#include "../utils/math/Vec3.h"
#include "../world/block/BlockPos.h"
#include "Direction.h"

class HitResult {
public:
    enum Type { MISS, BLOCK, ENTITY };

    HitResult();
    ~HitResult();

    void setMiss();
    void setBlock(const Vec3 &hitPos, const BlockPos &pos, Direction *face, float distance);
    void setEntity(const Vec3 &hitPos, const UUID &uuid, uint64_t type, float distance);

    Type getType() const;

    bool isMiss() const;
    bool isBlock() const;
    bool isEntity() const;

    const Vec3 &getHitPos() const;
    float getDistance() const;

    const BlockPos &getBlockPos() const;
    Direction *getBlockFace() const;

    const UUID &getEntityUUID() const;
    uint64_t getEntityType() const;

private:
    Type m_type;

    Vec3 m_hitPos;
    float m_distance;

    BlockPos m_blockPos;
    Direction *m_blockFace;

    UUID m_entityUUID;
    uint64_t m_entityType;
};
