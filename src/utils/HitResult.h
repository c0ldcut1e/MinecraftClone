#pragma once

#include "../utils/UUID.h"
#include "../utils/math/Vec3.h"

class HitResult {
public:
    enum Type { MISS, BLOCK, ENTITY };

    HitResult();
    ~HitResult();

    void setMiss();
    void setBlock(const Vec3 &hitPos, int x, int y, int z, int face, float distance);
    void setEntity(const Vec3 &hitPos, const UUID &uuid, uint64_t type, float distance);

    Type getType() const;

    bool isMiss() const;
    bool isBlock() const;
    bool isEntity() const;

    const Vec3 &getHitPos() const;
    float getDistance() const;

    int getBlockX() const;
    int getBlockY() const;
    int getBlockZ() const;
    int getBlockFace() const;

    const UUID &getEntityUUID() const;
    uint64_t getEntityType() const;

private:
    Type m_type;

    Vec3 m_hitPos;
    float m_distance;

    int m_blockX;
    int m_blockY;
    int m_blockZ;
    int m_blockFace;

    UUID m_entityUUID;
    uint64_t m_entityType;
};
