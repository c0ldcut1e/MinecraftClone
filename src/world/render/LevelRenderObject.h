#pragma once

#include <cstdint>
#include <string>

#include "../../utils/AABB.h"
#include "../chunk/ChunkPos.h"

enum class LevelRenderObjectType : uint8_t
{
    QUAD,
    BILLBOARD,
    LINE,
    BOX
};

enum class LevelRenderTextureSource : uint8_t
{
    NONE,
    BLOCK_ATLAS,
    PARTICLE_ATLAS,
    PATH
};

enum class LevelRenderObjectStage : uint8_t
{
    BEFORE_CLOUDS,
    AFTER_CLOUDS,
    AFTER_ENTITIES,
    AFTER_FOG
};

struct LevelRenderObject
{
    uint64_t id                            = 0;
    LevelRenderObjectType type             = LevelRenderObjectType::QUAD;
    LevelRenderTextureSource textureSource = LevelRenderTextureSource::NONE;
    LevelRenderObjectStage stage           = LevelRenderObjectStage::BEFORE_CLOUDS;
    std::string texturePath;
    ChunkPos chunkPos;
    bool renderInChunk = false;
    bool blend         = false;
    bool depthTest     = true;
    bool depthWrite    = true;
    bool cull          = false;
    bool filled        = true;
    int renderOrder    = 0;
    float alphaCutoff  = -1.0f;
    float lineWidth    = 1.0f;
    uint32_t color     = 0xFFFFFFFF;
    Vec3 p1;
    Vec3 p2;
    Vec3 p3;
    Vec3 p4;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};
