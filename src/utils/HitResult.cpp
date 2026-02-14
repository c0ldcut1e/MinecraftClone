#include "HitResult.h"

HitResult::HitResult() : m_type(MISS), m_hitPos(0.0, 0.0, 0.0), m_distance(0.0f), m_blockPos(), m_blockFace(nullptr), m_entityUUID(), m_entityType(0) {}

HitResult::~HitResult() {}

void HitResult::setMiss() {
    m_type       = MISS;
    m_hitPos     = Vec3(0.0, 0.0, 0.0);
    m_distance   = 0.0f;
    m_blockPos   = BlockPos();
    m_blockFace  = nullptr;
    m_entityUUID = UUID();
    m_entityType = 0;
}

void HitResult::setBlock(const Vec3 &hitPos, const BlockPos &pos, Direction *face, float distance) {
    m_type       = BLOCK;
    m_hitPos     = hitPos;
    m_distance   = distance;
    m_blockPos   = pos;
    m_blockFace  = face;
    m_entityUUID = UUID();
    m_entityType = 0;
}

void HitResult::setEntity(const Vec3 &hitPos, const UUID &uuid, uint64_t type, float distance) {
    m_type       = ENTITY;
    m_hitPos     = hitPos;
    m_distance   = distance;
    m_blockPos   = BlockPos();
    m_blockFace  = nullptr;
    m_entityUUID = uuid;
    m_entityType = type;
}

HitResult::Type HitResult::getType() const { return m_type; }

bool HitResult::isMiss() const { return m_type == MISS; }

bool HitResult::isBlock() const { return m_type == BLOCK; }

bool HitResult::isEntity() const { return m_type == ENTITY; }

const Vec3 &HitResult::getHitPos() const { return m_hitPos; }

float HitResult::getDistance() const { return m_distance; }

const BlockPos &HitResult::getBlockPos() const { return m_blockPos; }

Direction *HitResult::getBlockFace() const { return m_blockFace; }

const UUID &HitResult::getEntityUUID() const { return m_entityUUID; }

uint64_t HitResult::getEntityType() const { return m_entityType; }
