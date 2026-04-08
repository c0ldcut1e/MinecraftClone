#include "ChunkMesher.h"

#include <cmath>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "../../utils/Direction.h"
#include "../../utils/math/Mth.h"
#include "../LevelSource.h"
#include "../block/Block.h"
#include "../lighting/LightEngine.h"

struct MaskCell
{
    Texture *texture;
    Block::UVRect atlasRect;
    uint32_t tint;
    uint16_t rawLight;
    bool filled;
};

struct VertexLight
{
    uint16_t rawLight;
    float r;
    float g;
    float b;
};

struct MeshBucket
{
    std::vector<float> vertices;
    std::vector<float> atlasRects;
    std::vector<uint16_t> rawLights;
    std::vector<float> shades;
    std::vector<uint32_t> tints;
};

struct WallMountedVertex
{
    float x;
    float y;
    float z;
};

static constexpr int BUILD_CACHE_X     = Chunk::SIZE_X + 2;
static constexpr int BUILD_CACHE_Y     = Chunk::SIZE_Y + 2;
static constexpr int BUILD_CACHE_Z     = Chunk::SIZE_Z + 2;
static constexpr int BUILD_CACHE_SIZE  = BUILD_CACHE_X * BUILD_CACHE_Y * BUILD_CACHE_Z;
static constexpr int SMOOTH_CACHE_X    = Chunk::SIZE_X + 1;
static constexpr int SMOOTH_CACHE_Y    = Chunk::SIZE_Y + 1;
static constexpr int SMOOTH_CACHE_Z    = Chunk::SIZE_Z + 1;
static constexpr int SMOOTH_CACHE_SIZE = 6 * SMOOTH_CACHE_X * SMOOTH_CACHE_Y * SMOOTH_CACHE_Z;

struct BuildData
{
    BuildData(Level *level, const Chunk *chunk, bool cacheRawLight)
        : level(level), chunk(chunk), chunkPos(chunk->getPos()), baseX(chunkPos.x * Chunk::SIZE_X),
          baseY(chunkPos.y * Chunk::SIZE_Y), baseZ(chunkPos.z * Chunk::SIZE_Z),
          cacheRawLight(cacheRawLight), rawLights(), rawLoaded(), solids(), smoothLightSums(),
          smoothLightMeta()
    {
        for (int dz = -1; dz <= 1; dz++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0 && dz == 0)
                    {
                        chunks[dx + 1][dy + 1][dz + 1] = chunk;
                    }
                    else
                    {
                        chunks[dx + 1][dy + 1][dz + 1] = level->getChunk(
                                ChunkPos(chunkPos.x + dx, chunkPos.y + dy, chunkPos.z + dz));
                    }
                }
            }
        }

        solids.resize(BUILD_CACHE_SIZE);

        if (cacheRawLight)
        {
            rawLights.resize(BUILD_CACHE_SIZE);
            rawLoaded.resize(BUILD_CACHE_SIZE);
            smoothLightSums.resize(SMOOTH_CACHE_SIZE);
            smoothLightMeta.resize(SMOOTH_CACHE_SIZE);
        }
    }

    Level *level;
    const Chunk *chunk;
    ChunkPos chunkPos;
    int baseX;
    int baseY;
    int baseZ;
    bool cacheRawLight;
    const Chunk *chunks[3][3][3];
    std::vector<uint16_t> rawLights;
    std::vector<uint8_t> rawLoaded;
    std::vector<uint8_t> solids;
    std::vector<uint32_t> smoothLightSums;
    std::vector<uint8_t> smoothLightMeta;
};

typedef std::function<void(int, int, int, int, Texture *, const Block::UVRect &, uint16_t,
                           uint32_t)>
        GreedyEmitRect;

typedef std::function<void(Direction *, Texture *, const Block::UVRect &, uint16_t, uint32_t, float,
                           float, float, float, float, float)>
        EmitFace;

typedef std::function<void(Direction *, Texture *, const Block::UVRect &, uint16_t, uint32_t, float,
                           float, float, float, float, float, float, float, float, float, float,
                           float)>
        EmitFace4;

typedef std::function<void(Direction *, Texture *, const Block::UVRect &, uint16_t, uint32_t, float,
                           float, float, float, float, float, float, float, float, float)>
        EmitFixedUvFace;

typedef std::function<void(Direction *, Texture *, const Block::UVRect &, uint16_t, uint32_t, float,
                           float, float, float, float, float, float, float, float, float, float,
                           float, float, float, float, float)>
        EmitFace4FixedUv;

static constexpr float AO_STRENGTH_MIN   = 0.0f;
static constexpr float AO_STRENGTH_MAX   = 1.0f;
static constexpr float AO_STRENGTH_VALUE = 0.2f;

static constexpr uint8_t TINT_MODE_MASKED = 255;

static inline uint8_t getTintMode(uint32_t tintPacked)
{
    return (uint8_t) ((tintPacked >> 24) & 0xFF);
}

static inline uint32_t packTintMode(uint32_t tintRgb, uint8_t mode)
{
    return (tintRgb & 0x00FFFFFFu) | ((uint32_t) mode << 24);
}

static inline float getAoStrength()
{
    float t = Mth::clampf(AO_STRENGTH_VALUE, 0.0f, 1.0f);
    return AO_STRENGTH_MIN + (AO_STRENGTH_MAX - AO_STRENGTH_MIN) * t;
}

static inline float getDirectionalShade(Direction *direction)
{
    float occlusion = 0.0f;
    if (direction == Direction::NORTH || direction == Direction::SOUTH)
    {
        occlusion = 0.2f;
    }
    else if (direction == Direction::EAST || direction == Direction::WEST)
    {
        occlusion = 0.4f;
    }
    else if (direction == Direction::DOWN)
    {
        occlusion = 0.5f;
    }

    return 1.0f - occlusion * getAoStrength();
}

static inline float getOldDirectionalShade(Direction *direction)
{
    if (direction == Direction::NORTH || direction == Direction::SOUTH)
    {
        return 0.8f;
    }
    if (direction == Direction::EAST || direction == Direction::WEST)
    {
        return 0.6f;
    }
    if (direction == Direction::DOWN)
    {
        return 0.5f;
    }

    return 1.0f;
}

static inline uint16_t packLight(uint8_t r, uint8_t g, uint8_t b)
{
    if (r > 15)
    {
        r = 15;
    }
    if (g > 15)
    {
        g = 15;
    }
    if (b > 15)
    {
        b = 15;
    }
    return (uint16_t) ((r << 8) | (g << 4) | (b));
}

static inline void unpackLight(uint16_t k, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t) ((k >> 8) & 0xF);
    *g = (uint8_t) ((k >> 4) & 0xF);
    *b = (uint8_t) (k & 0xF);
}

static inline uint16_t packRawLight(uint8_t br, uint8_t bg, uint8_t bb, uint8_t sky)
{
    return (uint16_t) ((br & 15) | ((bg & 15) << 4) | ((bb & 15) << 8) | ((sky & 15) << 12));
}

static inline uint32_t packSmoothLightSums(uint8_t br, uint8_t bg, uint8_t bb, uint8_t sky)
{
    return (uint32_t) br | ((uint32_t) bg << 6) | ((uint32_t) bb << 12) | ((uint32_t) sky << 18);
}

static inline uint8_t unpackSmoothLightSum(uint32_t packed, int shift)
{
    return (uint8_t) ((packed >> shift) & 63u);
}

static inline void push(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                        std::vector<float> &atlasRects, std::vector<float> &shades,
                        std::vector<uint32_t> &tints, float x, float y, float z, float u, float v,
                        float r, float g, float b, uint16_t rawLight, float shadeMul, uint32_t tint,
                        const Block::UVRect &atlasRect)
{
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(u);
    vertices.push_back(v);
    vertices.push_back(r);
    vertices.push_back(g);
    vertices.push_back(b);

    atlasRects.push_back(atlasRect.u0);
    atlasRects.push_back(atlasRect.v0);
    atlasRects.push_back(atlasRect.u1);
    atlasRects.push_back(atlasRect.v1);

    rawLights.push_back(rawLight);
    shades.push_back(shadeMul);
    tints.push_back(tint);
}

static inline bool sameUVRect(const Block::UVRect &a, const Block::UVRect &b)
{
    return a.u0 == b.u0 && a.v0 == b.v0 && a.u1 == b.u1 && a.v1 == b.v1;
}

static inline void shade(uint8_t lr, uint8_t lg, uint8_t lb, float shadeMul, float *r, float *g,
                         float *b)
{
    float minLight = 0.03f;
    float baseR    = (float) lr / 15.0f;
    float baseG    = (float) lg / 15.0f;
    float baseB    = (float) lb / 15.0f;

    if (baseR < minLight)
    {
        baseR = minLight;
    }
    if (baseG < minLight)
    {
        baseG = minLight;
    }
    if (baseB < minLight)
    {
        baseB = minLight;
    }

    *r = baseR * shadeMul;
    *g = baseG * shadeMul;
    *b = baseB * shadeMul;
}

static inline void addFace(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                           std::vector<float> &atlasRects, std::vector<float> &shades,
                           std::vector<uint32_t> &tints, float x1, float y1, float z1, float x2,
                           float y2, float z2, float x3, float y3, float z3, float x4, float y4,
                           float z4, float uMax, float vMax, float r, float g, float b,
                           uint16_t rawLight, float shadeMul, uint32_t tint,
                           const Block::UVRect &atlasRect)
{
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, uMax, vMax, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, uMax, 0.0f, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, 0.0f, 0.0f, r, g, b, rawLight,
         shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, 0.0f, 0.0f, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, 0.0f, vMax, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, uMax, vMax, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
}

static inline void addFaceUV(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                             std::vector<float> &atlasRects, std::vector<float> &shades,
                             std::vector<uint32_t> &tints, float x1, float y1, float z1, float x2,
                             float y2, float z2, float x3, float y3, float z3, float x4, float y4,
                             float z4, float u0, float v0, float u1, float v1, float r, float g,
                             float b, uint16_t rawLight, float shadeMul, uint32_t tint,
                             const Block::UVRect &atlasRect)
{
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
}

static inline void addFace4UV(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                              std::vector<float> &atlasRects, std::vector<float> &shades,
                              std::vector<uint32_t> &tints, float x1, float y1, float z1, float x2,
                              float y2, float z2, float x3, float y3, float z3, float x4, float y4,
                              float z4, float u0, float v0, float u1, float v1, float r, float g,
                              float b, uint16_t rawLight, float shadeMul, uint32_t tint,
                              const Block::UVRect &atlasRect)
{
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, r, g, b, rawLight,
         shadeMul, tint, atlasRect);
}

static inline void addFaceSmooth(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                                 std::vector<float> &atlasRects, std::vector<float> &shades,
                                 std::vector<uint32_t> &tints, float x1, float y1, float z1,
                                 float x2, float y2, float z2, float x3, float y3, float z3,
                                 float x4, float y4, float z4, float uMax, float vMax,
                                 const VertexLight &l1, const VertexLight &l2,
                                 const VertexLight &l3, const VertexLight &l4, float shadeMul,
                                 uint32_t tint, const Block::UVRect &atlasRect)
{
    float b1    = l1.r + l1.g + l1.b;
    float b2    = l2.r + l2.g + l2.b;
    float b3    = l3.r + l3.g + l3.b;
    float b4    = l4.r + l4.g + l4.b;
    bool diag13 = fabsf(b1 - b3) <= fabsf(b2 - b4);

    if (diag13)
    {
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, uMax, vMax, l1.r, l1.g,
             l1.b, l1.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, uMax, 0.0f, l2.r, l2.g,
             l2.b, l2.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, 0.0f, 0.0f, l3.r, l3.g,
             l3.b, l3.rawLight, shadeMul, tint, atlasRect);

        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, 0.0f, 0.0f, l3.r, l3.g,
             l3.b, l3.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, 0.0f, vMax, l4.r, l4.g,
             l4.b, l4.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, uMax, vMax, l1.r, l1.g,
             l1.b, l1.rawLight, shadeMul, tint, atlasRect);
        return;
    }

    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, uMax, 0.0f, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, 0.0f, 0.0f, l3.r, l3.g, l3.b,
         l3.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, 0.0f, vMax, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, 0.0f, vMax, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, uMax, vMax, l1.r, l1.g, l1.b,
         l1.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, uMax, 0.0f, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
}

static inline void addFaceUVSmooth(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                                   std::vector<float> &atlasRects, std::vector<float> &shades,
                                   std::vector<uint32_t> &tints, float x1, float y1, float z1,
                                   float x2, float y2, float z2, float x3, float y3, float z3,
                                   float x4, float y4, float z4, float u0, float v0, float u1,
                                   float v1, const VertexLight &l1, const VertexLight &l2,
                                   const VertexLight &l3, const VertexLight &l4, float shadeMul,
                                   uint32_t tint, const Block::UVRect &atlasRect)
{
    float b1    = l1.r + l1.g + l1.b;
    float b2    = l2.r + l2.g + l2.b;
    float b3    = l3.r + l3.g + l3.b;
    float b4    = l4.r + l4.g + l4.b;
    bool diag13 = fabsf(b1 - b3) <= fabsf(b2 - b4);

    if (diag13)
    {
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
             l1.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
             l2.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
             l3.rawLight, shadeMul, tint, atlasRect);

        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
             l3.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
             l4.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
             l1.rawLight, shadeMul, tint, atlasRect);
        return;
    }

    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
         l3.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
         l1.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
}

static inline void addFace4UVSmooth(std::vector<float> &vertices, std::vector<uint16_t> &rawLights,
                                    std::vector<float> &atlasRects, std::vector<float> &shades,
                                    std::vector<uint32_t> &tints, float x1, float y1, float z1,
                                    float x2, float y2, float z2, float x3, float y3, float z3,
                                    float x4, float y4, float z4, float u0, float v0, float u1,
                                    float v1, const VertexLight &l1, const VertexLight &l2,
                                    const VertexLight &l3, const VertexLight &l4, float shadeMul,
                                    uint32_t tint, const Block::UVRect &atlasRect)
{
    float b1    = l1.r + l1.g + l1.b;
    float b2    = l2.r + l2.g + l2.b;
    float b3    = l3.r + l3.g + l3.b;
    float b4    = l4.r + l4.g + l4.b;
    bool diag13 = fabsf(b1 - b3) <= fabsf(b2 - b4);

    if (diag13)
    {
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
             l1.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
             l2.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
             l3.rawLight, shadeMul, tint, atlasRect);

        push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
             l3.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
             l4.rawLight, shadeMul, tint, atlasRect);
        push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
             l1.rawLight, shadeMul, tint, atlasRect);
        return;
    }

    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x3, y3, z3, u0, v0, l3.r, l3.g, l3.b,
         l3.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);

    push(vertices, rawLights, atlasRects, shades, tints, x4, y4, z4, u0, v1, l4.r, l4.g, l4.b,
         l4.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x1, y1, z1, u1, v1, l1.r, l1.g, l1.b,
         l1.rawLight, shadeMul, tint, atlasRect);
    push(vertices, rawLights, atlasRects, shades, tints, x2, y2, z2, u1, v0, l2.r, l2.g, l2.b,
         l2.rawLight, shadeMul, tint, atlasRect);
}

static inline Direction *decodeDirection(uint8_t id)
{
    if (id == 1)
    {
        return Direction::NORTH;
    }
    if (id == 2)
    {
        return Direction::SOUTH;
    }
    if (id == 3)
    {
        return Direction::WEST;
    }
    if (id == 4)
    {
        return Direction::EAST;
    }
    if (id == 5)
    {
        return Direction::DOWN;
    }
    if (id == 6)
    {
        return Direction::UP;
    }
    return nullptr;
}

static inline uint16_t sampleLightKey(Level *level, const Chunk *chunk, int wx, int wy, int wz)
{
    LevelSource source(level);

    int cx = Mth::floorDiv(wx, Chunk::SIZE_X);
    int cy = Mth::floorDiv(wy, Chunk::SIZE_Y);
    int cz = Mth::floorDiv(wz, Chunk::SIZE_Z);

    int lx = Mth::floorMod(wx, Chunk::SIZE_X);
    int ly = Mth::floorMod(wy, Chunk::SIZE_Y);
    int lz = Mth::floorMod(wz, Chunk::SIZE_Z);

    const Chunk *_chunk = chunk;
    ChunkPos chunkPos   = chunk->getPos();

    if (cx != chunkPos.x || cy != chunkPos.y || cz != chunkPos.z)
    {
        _chunk = source.getChunk(ChunkPos(cx, cy, cz));
        if (!_chunk)
        {
            return packRawLight(0, 0, 0, 0);
        }
    }

    uint8_t br;
    uint8_t bg;
    uint8_t bb;
    _chunk->getBlockLight(lx, ly, lz, &br, &bg, &bb);

    uint8_t sky = _chunk->getSkyLight(lx, ly, lz);
    return packRawLight(br, bg, bb, sky);
}

static inline bool sampleLightKeyIfLoaded(Level *level, const Chunk *chunk, int wx, int wy, int wz,
                                          uint16_t *outRawLight)
{
    LevelSource source(level);

    int cx = Mth::floorDiv(wx, Chunk::SIZE_X);
    int cy = Mth::floorDiv(wy, Chunk::SIZE_Y);
    int cz = Mth::floorDiv(wz, Chunk::SIZE_Z);

    int lx = Mth::floorMod(wx, Chunk::SIZE_X);
    int ly = Mth::floorMod(wy, Chunk::SIZE_Y);
    int lz = Mth::floorMod(wz, Chunk::SIZE_Z);

    const Chunk *_chunk = chunk;
    ChunkPos chunkPos   = chunk->getPos();

    if (cx != chunkPos.x || cy != chunkPos.y || cz != chunkPos.z)
    {
        _chunk = source.getChunk(ChunkPos(cx, cy, cz));
        if (!_chunk)
        {
            return false;
        }
    }

    uint8_t br;
    uint8_t bg;
    uint8_t bb;
    _chunk->getBlockLight(lx, ly, lz, &br, &bg, &bb);

    uint8_t sky  = _chunk->getSkyLight(lx, ly, lz);
    *outRawLight = packRawLight(br, bg, bb, sky);
    return true;
}

static inline void decodeFaceBasis(Direction *direction, int *nx, int *ny, int *nz, int *ux,
                                   int *uy, int *uz, int *vx, int *vy, int *vz)
{
    *nx = direction->dx;
    *ny = direction->dy;
    *nz = direction->dz;

    if (direction == Direction::UP || direction == Direction::DOWN)
    {
        *ux = 1;
        *uy = 0;
        *uz = 0;
        *vx = 0;
        *vy = 0;
        *vz = 1;
        return;
    }
    if (direction == Direction::NORTH || direction == Direction::SOUTH)
    {
        *ux = 1;
        *uy = 0;
        *uz = 0;
        *vx = 0;
        *vy = 1;
        *vz = 0;
        return;
    }

    *ux = 0;
    *uy = 1;
    *uz = 0;
    *vx = 0;
    *vy = 0;
    *vz = 1;
}

static inline float getShadeMul(Direction *direction, bool smoothLighting)
{
    if (!smoothLighting)
    {
        return getOldDirectionalShade(direction);
    }

    return getDirectionalShade(direction);
}

static inline void colorFromRawLight(Level *level, uint16_t rawLight, uint32_t tint, float shadeMul,
                                     float *r, float *g, float *b)
{
    uint8_t br    = (uint8_t) (rawLight & 15);
    uint8_t bg    = (uint8_t) ((rawLight >> 4) & 15);
    uint8_t bb    = (uint8_t) ((rawLight >> 8) & 15);
    uint8_t sky   = (uint8_t) ((rawLight >> 12) & 15);
    uint8_t clamp = level->getSkyLightClamp();
    if (sky > clamp)
    {
        sky = clamp;
    }

    uint8_t lr = br > sky ? br : sky;
    uint8_t lg = bg > sky ? bg : sky;
    uint8_t lb = bb > sky ? bb : sky;

    shade(lr, lg, lb, shadeMul, r, g, b);

    *r *= (float) ((tint >> 16) & 0xFF) / 255.0f;
    *g *= (float) ((tint >> 8) & 0xFF) / 255.0f;
    *b *= (float) (tint & 0xFF) / 255.0f;
}

static inline uint16_t sampleSmoothRawLight(Level *level, const Chunk *chunk, Direction *direction,
                                            uint16_t fallbackRawLight, float x, float y, float z)
{
    int nx;
    int ny;
    int nz;
    int ux;
    int uy;
    int uz;
    int vx;
    int vy;
    int vz;
    decodeFaceBasis(direction, &nx, &ny, &nz, &ux, &uy, &uz, &vx, &vy, &vz);

    int bx = (int) floorf(x - 0.5f * (float) nx);
    int by = (int) floorf(y - 0.5f * (float) ny);
    int bz = (int) floorf(z - 0.5f * (float) nz);

    float cx = (float) bx + 0.5f;
    float cy = (float) by + 0.5f;
    float cz = (float) bz + 0.5f;

    float du = 0.0f;
    if (ux != 0)
        du = x - cx;
    else if (uy != 0)
        du = y - cy;
    else
        du = z - cz;

    float dv = 0.0f;
    if (vx != 0)
        dv = x - cx;
    else if (vy != 0)
        dv = y - cy;
    else
        dv = z - cz;

    int su = du >= 0.0f ? 1 : -1;
    int sv = dv >= 0.0f ? 1 : -1;

    uint16_t s0 = fallbackRawLight;
    uint16_t s1 = fallbackRawLight;
    uint16_t s2 = fallbackRawLight;
    uint16_t s3 = fallbackRawLight;

    (void) sampleLightKeyIfLoaded(level, chunk, bx + nx, by + ny, bz + nz, &s0);
    (void) sampleLightKeyIfLoaded(level, chunk, bx + nx + su * ux, by + ny + su * uy,
                                  bz + nz + su * uz, &s1);
    (void) sampleLightKeyIfLoaded(level, chunk, bx + nx + sv * vx, by + ny + sv * vy,
                                  bz + nz + sv * vz, &s2);
    (void) sampleLightKeyIfLoaded(level, chunk, bx + nx + su * ux + sv * vx,
                                  by + ny + su * uy + sv * vy, bz + nz + su * uz + sv * vz, &s3);

    float br = (float) ((s0 & 15) + (s1 & 15) + (s2 & 15) + (s3 & 15)) * 0.25f;
    float bg = (float) (((s0 >> 4) & 15) + ((s1 >> 4) & 15) + ((s2 >> 4) & 15) + ((s3 >> 4) & 15)) *
               0.25f;
    float bb = (float) (((s0 >> 8) & 15) + ((s1 >> 8) & 15) + ((s2 >> 8) & 15) + ((s3 >> 8) & 15)) *
               0.25f;
    float sk = (float) (((s0 >> 12) & 15) + ((s1 >> 12) & 15) + ((s2 >> 12) & 15) +
                        ((s3 >> 12) & 15)) *
               0.25f;

    uint8_t outBr = (uint8_t) Mth::clampf(br + 0.5f, 0.0f, 15.0f);
    uint8_t outBg = (uint8_t) Mth::clampf(bg + 0.5f, 0.0f, 15.0f);
    uint8_t outBb = (uint8_t) Mth::clampf(bb + 0.5f, 0.0f, 15.0f);
    uint8_t outSk = (uint8_t) Mth::clampf(sk + 0.5f, 0.0f, 15.0f);
    return packRawLight(outBr, outBg, outBb, outSk);
}

static inline VertexLight buildVertexLight(Level *level, const Chunk *chunk, Direction *direction,
                                           bool smoothLighting, uint16_t rawLightFallback,
                                           uint32_t tint, float x, float y, float z)
{
    VertexLight light;
    light.rawLight = rawLightFallback;

    if (smoothLighting)
    {
        light.rawLight = sampleSmoothRawLight(level, chunk, direction, rawLightFallback, x, y, z);
    }

    uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
    colorFromRawLight(level, light.rawLight, tintForLighting, getDirectionalShade(direction),
                      &light.r, &light.g, &light.b);

    return light;
}

static inline Block *getBlockLevel(Level *level, const Chunk *chunk, const BlockPos &pos)
{
    LevelSource source(level);

    if (pos.y < 0 || pos.y >= Chunk::SIZE_Y)
    {
        return nullptr;
    }
    if (pos.x >= 0 && pos.x < Chunk::SIZE_X && pos.z >= 0 && pos.z < Chunk::SIZE_Z)
    {
        return Block::byId(chunk->getBlockId(pos.x, pos.y, pos.z));
    }

    ChunkPos chunkPos = chunk->getPos();
    int lx            = pos.x;
    int lz            = pos.z;

    if (lx < 0)
    {
        chunkPos.x -= 1;
        lx += Chunk::SIZE_X;
    }
    else if (lx >= Chunk::SIZE_X)
    {
        chunkPos.x += 1;
        lx -= Chunk::SIZE_X;
    }

    if (lz < 0)
    {
        chunkPos.z -= 1;
        lz += Chunk::SIZE_Z;
    }
    else if (lz >= Chunk::SIZE_Z)
    {
        chunkPos.z += 1;
        lz -= Chunk::SIZE_Z;
    }

    const Chunk *_chunk = source.getChunk(chunkPos);
    if (!_chunk)
    {
        return nullptr;
    }

    return Block::byId(_chunk->getBlockId(lx, pos.y, lz));
}

static inline bool isSolidLevel(Level *level, const Chunk *chunk, const BlockPos &pos)
{
    if (pos.y < 0 || pos.y >= Chunk::SIZE_Y)
    {
        return false;
    }

    Block *block = getBlockLevel(level, chunk, pos);
    if (!block)
    {
        return level->areEmptyChunksSolid();
    }
    return block->isSolid();
}

static inline int getBuildCacheIndex(int x, int y, int z)
{
    return (x + 1) + BUILD_CACHE_X * ((y + 1) + BUILD_CACHE_Y * (z + 1));
}

static inline bool isInsideBuildCache(int x, int y, int z)
{
    return x >= -1 && x <= Chunk::SIZE_X && y >= -1 && y <= Chunk::SIZE_Y && z >= -1 &&
           z <= Chunk::SIZE_Z;
}

static inline int directionToSmoothCacheLayer(Direction *direction)
{
    if (direction == Direction::NORTH)
    {
        return 0;
    }
    if (direction == Direction::SOUTH)
    {
        return 1;
    }
    if (direction == Direction::WEST)
    {
        return 2;
    }
    if (direction == Direction::EAST)
    {
        return 3;
    }
    if (direction == Direction::DOWN)
    {
        return 4;
    }
    if (direction == Direction::UP)
    {
        return 5;
    }

    return -1;
}

static inline int getSmoothCacheIndex(int layer, int x, int y, int z)
{
    int vertexIndex = x + SMOOTH_CACHE_X * (y + SMOOTH_CACHE_Y * z);
    return vertexIndex + layer * SMOOTH_CACHE_X * SMOOTH_CACHE_Y * SMOOTH_CACHE_Z;
}

static inline bool isNearlyIntegral(float value) { return fabsf(value - roundf(value)) <= 0.001f; }

static inline const Chunk *getBuildChunk2D(const BuildData &buildData, int *x, int *z)
{
    int ox = 0;
    int oz = 0;

    if (*x < 0)
    {
        ox = -1;
        *x += Chunk::SIZE_X;
    }
    else if (*x >= Chunk::SIZE_X)
    {
        ox = 1;
        *x -= Chunk::SIZE_X;
    }

    if (*z < 0)
    {
        oz = -1;
        *z += Chunk::SIZE_Z;
    }
    else if (*z >= Chunk::SIZE_Z)
    {
        oz = 1;
        *z -= Chunk::SIZE_Z;
    }

    return buildData.chunks[ox + 1][1][oz + 1];
}

static inline const Chunk *getBuildChunk3D(const BuildData &buildData, int *x, int *y, int *z)
{
    int ox = 0;
    int oy = 0;
    int oz = 0;

    if (*x < 0)
    {
        ox = -1;
        *x += Chunk::SIZE_X;
    }
    else if (*x >= Chunk::SIZE_X)
    {
        ox = 1;
        *x -= Chunk::SIZE_X;
    }

    if (*y < 0)
    {
        oy = -1;
        *y += Chunk::SIZE_Y;
    }
    else if (*y >= Chunk::SIZE_Y)
    {
        oy = 1;
        *y -= Chunk::SIZE_Y;
    }

    if (*z < 0)
    {
        oz = -1;
        *z += Chunk::SIZE_Z;
    }
    else if (*z >= Chunk::SIZE_Z)
    {
        oz = 1;
        *z -= Chunk::SIZE_Z;
    }

    return buildData.chunks[ox + 1][oy + 1][oz + 1];
}

static void buildSolidCache(BuildData *buildData)
{
    for (int z = -1; z <= Chunk::SIZE_Z; z++)
    {
        for (int y = -1; y <= Chunk::SIZE_Y; y++)
        {
            for (int x = -1; x <= Chunk::SIZE_X; x++)
            {
                uint8_t solid = 0;

                if (y >= 0 && y < Chunk::SIZE_Y)
                {
                    int lx             = x;
                    int lz             = z;
                    const Chunk *chunk = getBuildChunk2D(*buildData, &lx, &lz);

                    solid = 1;
                    if (chunk)
                    {
                        Block *block = Block::byId(chunk->getBlockId(lx, y, lz));
                        solid        = block ? (block->isSolid() ? 1 : 0) : 1;
                    }
                }

                buildData->solids[(size_t) getBuildCacheIndex(x, y, z)] = solid;
            }
        }
    }
}

static void buildRawLightCache(BuildData *buildData)
{
    for (int z = -1; z <= Chunk::SIZE_Z; z++)
    {
        for (int y = -1; y <= Chunk::SIZE_Y; y++)
        {
            for (int x = -1; x <= Chunk::SIZE_X; x++)
            {
                int lx = x;
                int ly = y;
                int lz = z;

                size_t index       = (size_t) getBuildCacheIndex(x, y, z);
                const Chunk *chunk = getBuildChunk3D(*buildData, &lx, &ly, &lz);
                if (!chunk)
                {
                    buildData->rawLights[index] = packRawLight(0, 0, 0, 0);
                    buildData->rawLoaded[index] = 0;
                    continue;
                }

                uint8_t br;
                uint8_t bg;
                uint8_t bb;
                chunk->getBlockLight(lx, ly, lz, &br, &bg, &bb);

                buildData->rawLights[index] =
                        packRawLight(br, bg, bb, chunk->getSkyLight(lx, ly, lz));
                buildData->rawLoaded[index] = 1;
            }
        }
    }
}

static inline uint16_t sampleLightKey(const BuildData &buildData, int wx, int wy, int wz)
{
    if (buildData.cacheRawLight)
    {
        int x = wx - buildData.baseX;
        int y = wy - buildData.baseY;
        int z = wz - buildData.baseZ;
        if (isInsideBuildCache(x, y, z))
        {
            return buildData.rawLights[(size_t) getBuildCacheIndex(x, y, z)];
        }
    }

    return sampleLightKey(buildData.level, buildData.chunk, wx, wy, wz);
}

static inline bool sampleLightKeyIfLoaded(const BuildData &buildData, int wx, int wy, int wz,
                                          uint16_t *outRawLight)
{
    if (buildData.cacheRawLight)
    {
        int x = wx - buildData.baseX;
        int y = wy - buildData.baseY;
        int z = wz - buildData.baseZ;
        if (isInsideBuildCache(x, y, z))
        {
            size_t index = (size_t) getBuildCacheIndex(x, y, z);
            *outRawLight = buildData.rawLights[index];
            return buildData.rawLoaded[index] != 0;
        }
    }

    return sampleLightKeyIfLoaded(buildData.level, buildData.chunk, wx, wy, wz, outRawLight);
}

static inline void accumulateSmoothLightSample(const BuildData &buildData, int wx, int wy, int wz,
                                               uint8_t *sumBr, uint8_t *sumBg, uint8_t *sumBb,
                                               uint8_t *sumSk, uint8_t *missingCount)
{
    uint16_t sample = 0;
    if (!sampleLightKeyIfLoaded(buildData, wx, wy, wz, &sample))
    {
        (*missingCount)++;
        return;
    }

    *sumBr = (uint8_t) (*sumBr + (sample & 15));
    *sumBg = (uint8_t) (*sumBg + ((sample >> 4) & 15));
    *sumBb = (uint8_t) (*sumBb + ((sample >> 8) & 15));
    *sumSk = (uint8_t) (*sumSk + ((sample >> 12) & 15));
}

static inline uint16_t resolveSmoothRawLight(uint32_t packedSums, uint8_t missingCount,
                                             uint16_t fallbackRawLight)
{
    int sumBr = (int) unpackSmoothLightSum(packedSums, 0) +
                (int) (fallbackRawLight & 15) * (int) missingCount;
    int sumBg = (int) unpackSmoothLightSum(packedSums, 6) +
                (int) ((fallbackRawLight >> 4) & 15) * (int) missingCount;
    int sumBb = (int) unpackSmoothLightSum(packedSums, 12) +
                (int) ((fallbackRawLight >> 8) & 15) * (int) missingCount;
    int sumSk = (int) unpackSmoothLightSum(packedSums, 18) +
                (int) ((fallbackRawLight >> 12) & 15) * (int) missingCount;

    return packRawLight((uint8_t) ((sumBr + 2) >> 2), (uint8_t) ((sumBg + 2) >> 2),
                        (uint8_t) ((sumBb + 2) >> 2), (uint8_t) ((sumSk + 2) >> 2));
}

static inline uint16_t sampleSmoothRawLight(const BuildData &buildData, Direction *direction,
                                            uint16_t fallbackRawLight, float x, float y, float z)
{
    int nx;
    int ny;
    int nz;
    int ux;
    int uy;
    int uz;
    int vx;
    int vy;
    int vz;
    decodeFaceBasis(direction, &nx, &ny, &nz, &ux, &uy, &uz, &vx, &vy, &vz);

    int bx = (int) floorf(x - 0.5f * (float) nx);
    int by = (int) floorf(y - 0.5f * (float) ny);
    int bz = (int) floorf(z - 0.5f * (float) nz);

    float cx = (float) bx + 0.5f;
    float cy = (float) by + 0.5f;
    float cz = (float) bz + 0.5f;

    float du = 0.0f;
    if (ux != 0)
        du = x - cx;
    else if (uy != 0)
        du = y - cy;
    else
        du = z - cz;

    float dv = 0.0f;
    if (vx != 0)
        dv = x - cx;
    else if (vy != 0)
        dv = y - cy;
    else
        dv = z - cz;

    int su = du >= 0.0f ? 1 : -1;
    int sv = dv >= 0.0f ? 1 : -1;

    uint16_t s0 = fallbackRawLight;
    uint16_t s1 = fallbackRawLight;
    uint16_t s2 = fallbackRawLight;
    uint16_t s3 = fallbackRawLight;

    (void) sampleLightKeyIfLoaded(buildData, bx + nx, by + ny, bz + nz, &s0);
    (void) sampleLightKeyIfLoaded(buildData, bx + nx + su * ux, by + ny + su * uy,
                                  bz + nz + su * uz, &s1);
    (void) sampleLightKeyIfLoaded(buildData, bx + nx + sv * vx, by + ny + sv * vy,
                                  bz + nz + sv * vz, &s2);
    (void) sampleLightKeyIfLoaded(buildData, bx + nx + su * ux + sv * vx,
                                  by + ny + su * uy + sv * vy, bz + nz + su * uz + sv * vz, &s3);

    float br = (float) ((s0 & 15) + (s1 & 15) + (s2 & 15) + (s3 & 15)) * 0.25f;
    float bg = (float) (((s0 >> 4) & 15) + ((s1 >> 4) & 15) + ((s2 >> 4) & 15) + ((s3 >> 4) & 15)) *
               0.25f;
    float bb = (float) (((s0 >> 8) & 15) + ((s1 >> 8) & 15) + ((s2 >> 8) & 15) + ((s3 >> 8) & 15)) *
               0.25f;
    float sk = (float) (((s0 >> 12) & 15) + ((s1 >> 12) & 15) + ((s2 >> 12) & 15) +
                        ((s3 >> 12) & 15)) *
               0.25f;

    uint8_t outBr = (uint8_t) Mth::clampf(br + 0.5f, 0.0f, 15.0f);
    uint8_t outBg = (uint8_t) Mth::clampf(bg + 0.5f, 0.0f, 15.0f);
    uint8_t outBb = (uint8_t) Mth::clampf(bb + 0.5f, 0.0f, 15.0f);
    uint8_t outSk = (uint8_t) Mth::clampf(sk + 0.5f, 0.0f, 15.0f);
    return packRawLight(outBr, outBg, outBb, outSk);
}

static inline bool sampleSmoothRawLightCached(BuildData &buildData, Direction *direction,
                                              uint16_t fallbackRawLight, float x, float y, float z,
                                              uint16_t *outRawLight)
{
    if (!buildData.cacheRawLight || buildData.smoothLightSums.empty() ||
        buildData.smoothLightMeta.empty())
    {
        return false;
    }

    if (!isNearlyIntegral(x) || !isNearlyIntegral(y) || !isNearlyIntegral(z))
    {
        return false;
    }

    int layer = directionToSmoothCacheLayer(direction);
    if (layer < 0)
    {
        return false;
    }

    int localX = (int) lroundf(x) - buildData.baseX;
    int localY = (int) lroundf(y) - buildData.baseY;
    int localZ = (int) lroundf(z) - buildData.baseZ;

    if (localX < 0 || localX > Chunk::SIZE_X || localY < 0 || localY > Chunk::SIZE_Y ||
        localZ < 0 || localZ > Chunk::SIZE_Z)
    {
        return false;
    }

    size_t index = (size_t) getSmoothCacheIndex(layer, localX, localY, localZ);
    uint8_t meta = buildData.smoothLightMeta[index];
    if ((meta & 0x80u) == 0)
    {
        int nx;
        int ny;
        int nz;
        int ux;
        int uy;
        int uz;
        int vx;
        int vy;
        int vz;
        decodeFaceBasis(direction, &nx, &ny, &nz, &ux, &uy, &uz, &vx, &vy, &vz);

        int bx = (int) floorf(x - 0.5f * (float) nx);
        int by = (int) floorf(y - 0.5f * (float) ny);
        int bz = (int) floorf(z - 0.5f * (float) nz);

        float cx = (float) bx + 0.5f;
        float cy = (float) by + 0.5f;
        float cz = (float) bz + 0.5f;

        float du = 0.0f;
        if (ux != 0)
            du = x - cx;
        else if (uy != 0)
            du = y - cy;
        else
            du = z - cz;

        float dv = 0.0f;
        if (vx != 0)
            dv = x - cx;
        else if (vy != 0)
            dv = y - cy;
        else
            dv = z - cz;

        int su = du >= 0.0f ? 1 : -1;
        int sv = dv >= 0.0f ? 1 : -1;

        uint8_t sumBr        = 0;
        uint8_t sumBg        = 0;
        uint8_t sumBb        = 0;
        uint8_t sumSk        = 0;
        uint8_t missingCount = 0;

        accumulateSmoothLightSample(buildData, bx + nx, by + ny, bz + nz, &sumBr, &sumBg, &sumBb,
                                    &sumSk, &missingCount);
        accumulateSmoothLightSample(buildData, bx + nx + su * ux, by + ny + su * uy,
                                    bz + nz + su * uz, &sumBr, &sumBg, &sumBb, &sumSk,
                                    &missingCount);
        accumulateSmoothLightSample(buildData, bx + nx + sv * vx, by + ny + sv * vy,
                                    bz + nz + sv * vz, &sumBr, &sumBg, &sumBb, &sumSk,
                                    &missingCount);
        accumulateSmoothLightSample(buildData, bx + nx + su * ux + sv * vx,
                                    by + ny + su * uy + sv * vy, bz + nz + su * uz + sv * vz,
                                    &sumBr, &sumBg, &sumBb, &sumSk, &missingCount);

        buildData.smoothLightSums[index] = packSmoothLightSums(sumBr, sumBg, sumBb, sumSk);
        buildData.smoothLightMeta[index] = (uint8_t) (0x80u | (missingCount & 0x7u));
        meta                             = buildData.smoothLightMeta[index];
    }

    *outRawLight = resolveSmoothRawLight(buildData.smoothLightSums[index], (uint8_t) (meta & 0x7u),
                                         fallbackRawLight);
    return true;
}

static inline VertexLight buildVertexLight(BuildData &buildData, Direction *direction,
                                           bool smoothLighting, uint16_t rawLightFallback,
                                           uint32_t tint, float x, float y, float z)
{
    VertexLight light;
    light.rawLight = rawLightFallback;

    if (smoothLighting)
    {
        if (!sampleSmoothRawLightCached(buildData, direction, rawLightFallback, x, y, z,
                                        &light.rawLight))
        {
            light.rawLight = sampleSmoothRawLight(buildData, direction, rawLightFallback, x, y, z);
        }
    }

    uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
    colorFromRawLight(buildData.level, light.rawLight, tintForLighting,
                      getDirectionalShade(direction), &light.r, &light.g, &light.b);

    return light;
}

static inline bool isSolidLevel(const BuildData &buildData, const BlockPos &pos)
{
    if (isInsideBuildCache(pos.x, pos.y, pos.z))
    {
        return buildData.solids[(size_t) getBuildCacheIndex(pos.x, pos.y, pos.z)] != 0;
    }

    return isSolidLevel(buildData.level, buildData.chunk, pos);
}

template<typename EmitRectFunc>
static void greedy2D(std::vector<MaskCell> &mask, int width, int height, bool useGreedy,
                     EmitRectFunc emitRect)
{
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            {
                MaskCell cell = mask[(size_t) i + (size_t) width * (size_t) j];
                if (!cell.filled || !cell.texture)
                {
                    continue;
                }

                int rWidth = 1;
                while (useGreedy && i + rWidth < width)
                {
                    MaskCell _cell = mask[(size_t) (i + rWidth) + (size_t) width * (size_t) j];
                    if (!_cell.filled || _cell.texture != cell.texture ||
                        !sameUVRect(_cell.atlasRect, cell.atlasRect) ||
                        _cell.rawLight != cell.rawLight || _cell.tint != cell.tint)
                    {
                        break;
                    }

                    rWidth++;
                }

                int rHeight = 1;
                bool ok     = true;
                while (useGreedy && j + rHeight < height && ok)
                {
                    for (int k = 0; k < rWidth; k++)
                    {
                        MaskCell _cell =
                                mask[(size_t) (i + k) + (size_t) width * (size_t) (j + rHeight)];
                        if (!_cell.filled || _cell.texture != cell.texture ||
                            !sameUVRect(_cell.atlasRect, cell.atlasRect) ||
                            _cell.rawLight != cell.rawLight || _cell.tint != cell.tint)
                        {
                            ok = false;
                            break;
                        }
                    }

                    if (ok)
                    {
                        rHeight++;
                    }
                }

                emitRect(i, j, rWidth, rHeight, cell.texture, cell.atlasRect, cell.rawLight,
                         cell.tint);

                for (int y = 0; y < rHeight; y++)
                {
                    for (int x = 0; x < rWidth; x++)
                    {
                        {
                            MaskCell &_cell =
                                    mask[(size_t) (i + x) + (size_t) width * (size_t) (j + y)];
                            _cell.filled    = false;
                            _cell.texture   = nullptr;
                            _cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                            _cell.rawLight  = 0;
                            _cell.tint      = 0xFFFFFF;
                        }
                    }
                }
            }
        }
    }
}
void ChunkMesher::buildMeshes(Level *level, const Chunk *chunk, bool smoothLighting,
                              bool grassSideOverlay, std::vector<MeshBuildResult> *outMeshes)
{
    std::unordered_map<Texture *, MeshBucket> buckets;
    BuildData buildData(level, chunk, smoothLighting);
    buildSolidCache(&buildData);
    if (smoothLighting)
    {
        buildRawLightCache(&buildData);
    }

    ChunkPos chunkPos = chunk->getPos();
    int baseX         = chunkPos.x * Chunk::SIZE_X;
    int baseY         = chunkPos.y * Chunk::SIZE_Y;
    int baseZ         = chunkPos.z * Chunk::SIZE_Z;
    Block *grassBlock = Block::byName("grass");

    EmitFace emit = [&](Direction *direction, Texture *texture, const Block::UVRect &atlasRect,
                        uint16_t rawLight, uint32_t tint, float x1, float y1, float z1, float x2,
                        float y2, float z2) {
        if (!texture)
        {
            return;
        }

        float uMax = 1.0f;
        float vMax = 1.0f;
        if (direction == Direction::NORTH || direction == Direction::SOUTH)
        {
            uMax = fabsf(x2 - x1);
            vMax = fabsf(y2 - y1);
        }
        else if (direction == Direction::EAST || direction == Direction::WEST)
        {
            uMax = fabsf(z2 - z1);
            vMax = fabsf(y2 - y1);
        }
        else
        {
            uMax = fabsf(x2 - x1);
            vMax = fabsf(z2 - z1);
        }

        float shadeMul = getShadeMul(direction, smoothLighting);
        float worldX1  = x1 + (float) baseX;
        float worldY1  = y1 + (float) baseY;
        float worldZ1  = z1 + (float) baseZ;
        float worldX2  = x2 + (float) baseX;
        float worldY2  = y2 + (float) baseY;
        float worldZ2  = z2 + (float) baseZ;

        MeshBucket &bucket = buckets[texture];
        if (!smoothLighting)
        {
            float r;
            float g;
            float b;
            uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
            colorFromRawLight(level, rawLight, tintForLighting, shadeMul, &r, &g, &b);

            if (direction == Direction::NORTH)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::SOUTH)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::UP)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::DOWN)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x1, y1, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::EAST)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::WEST)
            {
                addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                        bucket.tints, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, uMax, vMax, r,
                        g, b, rawLight, shadeMul, tint, atlasRect);
            }
            return;
        }

        if (direction == Direction::NORTH)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::SOUTH)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::UP)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::DOWN)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::EAST)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::WEST)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, uMax, vMax,
                          l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
    };

    EmitFace4 emitFace4 = [&](Direction *direction, Texture *texture,
                              const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint,
                              float x1, float y1, float z1, float x2, float y2, float z2, float x3,
                              float y3, float z3, float x4, float y4, float z4) {
        if (!texture)
        {
            return;
        }

        float shadeMul = getShadeMul(direction, smoothLighting);
        float worldX1  = x1 + (float) baseX;
        float worldY1  = y1 + (float) baseY;
        float worldZ1  = z1 + (float) baseZ;
        float worldX2  = x2 + (float) baseX;
        float worldY2  = y2 + (float) baseY;
        float worldZ2  = z2 + (float) baseZ;
        float worldX3  = x3 + (float) baseX;
        float worldY3  = y3 + (float) baseY;
        float worldZ3  = z3 + (float) baseZ;
        float worldX4  = x4 + (float) baseX;
        float worldY4  = y4 + (float) baseY;
        float worldZ4  = z4 + (float) baseZ;

        MeshBucket &bucket = buckets[texture];
        bool useSmooth     = false;
        if (!useSmooth)
        {
            float r;
            float g;
            float b;
            uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
            colorFromRawLight(level, rawLight, tintForLighting, shadeMul, &r, &g, &b);

            addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                    bucket.tints, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, 1.0f, 1.0f, r, g,
                    b, rawLight, shadeMul, tint, atlasRect);
            addFace(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                    bucket.tints, x4, y4, z4, x3, y3, z3, x2, y2, z2, x1, y1, z1, 1.0f, 1.0f, r, g,
                    b, rawLight, shadeMul, tint, atlasRect);
            return;
        }

        VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                          worldY1, worldZ1);
        VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                          worldY2, worldZ2);
        VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX3,
                                          worldY3, worldZ3);
        VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX4,
                                          worldY4, worldZ4);

        addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                      bucket.tints, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, 1.0f, 1.0f, l1,
                      l2, l3, l4, shadeMul, tint, atlasRect);
        addFaceSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                      bucket.tints, x4, y4, z4, x3, y3, z3, x2, y2, z2, x1, y1, z1, 1.0f, 1.0f, l4,
                      l3, l2, l1, shadeMul, tint, atlasRect);
    };

    EmitFixedUvFace emitFixedUV = [&](Direction *direction, Texture *texture,
                                      const Block::UVRect &atlasRect, uint16_t rawLight,
                                      uint32_t tint, float x1, float y1, float z1, float x2,
                                      float y2, float z2, float u0, float v0, float u1, float v1) {
        if (!texture)
        {
            return;
        }

        float shadeMul = getShadeMul(direction, smoothLighting);
        float worldX1  = x1 + (float) baseX;
        float worldY1  = y1 + (float) baseY;
        float worldZ1  = z1 + (float) baseZ;
        float worldX2  = x2 + (float) baseX;
        float worldY2  = y2 + (float) baseY;
        float worldZ2  = z2 + (float) baseZ;

        MeshBucket &bucket = buckets[texture];
        if (!smoothLighting)
        {
            float r;
            float g;
            float b;
            uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
            colorFromRawLight(level, rawLight, tintForLighting, shadeMul, &r, &g, &b);

            if (direction == Direction::NORTH)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::SOUTH)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::UP)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::DOWN)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::EAST)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            else if (direction == Direction::WEST)
            {
                addFaceUV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                          bucket.tints, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, u0, v0, u1,
                          v1, r, g, b, rawLight, shadeMul, tint, atlasRect);
            }
            return;
        }

        if (direction == Direction::NORTH)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::SOUTH)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::UP)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::DOWN)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x1, y1, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::EAST)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ1);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ1);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY2, worldZ2);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                              worldY1, worldZ2);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
        else if (direction == Direction::WEST)
        {
            VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ2);
            VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ2);
            VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY2, worldZ1);
            VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                              worldY1, worldZ1);
            addFaceUVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                            bucket.tints, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, u0, v0,
                            u1, v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
        }
    };

    EmitFace4FixedUv emitFace4FixedUV = [&](Direction *direction, Texture *texture,
                                            const Block::UVRect &atlasRect, uint16_t rawLight,
                                            uint32_t tint, float x1, float y1, float z1, float x2,
                                            float y2, float z2, float x3, float y3, float z3,
                                            float x4, float y4, float z4, float u0, float v0,
                                            float u1, float v1) {
        if (!texture)
        {
            return;
        }

        float shadeMul = getShadeMul(direction, smoothLighting);
        float worldX1  = x1 + (float) baseX;
        float worldY1  = y1 + (float) baseY;
        float worldZ1  = z1 + (float) baseZ;
        float worldX2  = x2 + (float) baseX;
        float worldY2  = y2 + (float) baseY;
        float worldZ2  = z2 + (float) baseZ;
        float worldX3  = x3 + (float) baseX;
        float worldY3  = y3 + (float) baseY;
        float worldZ3  = z3 + (float) baseZ;
        float worldX4  = x4 + (float) baseX;
        float worldY4  = y4 + (float) baseY;
        float worldZ4  = z4 + (float) baseZ;

        MeshBucket &bucket = buckets[texture];
        bool useSmooth     = false;
        if (!useSmooth)
        {
            float r;
            float g;
            float b;
            uint32_t tintForLighting = getTintMode(tint) == TINT_MODE_MASKED ? 0xFFFFFFu : tint;
            colorFromRawLight(level, rawLight, tintForLighting, shadeMul, &r, &g, &b);

            addFace4UV(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                       bucket.tints, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, u0, v0, u1, v1,
                       r, g, b, rawLight, shadeMul, tint, atlasRect);
            return;
        }

        VertexLight l1 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX1,
                                          worldY1, worldZ1);
        VertexLight l2 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX2,
                                          worldY2, worldZ2);
        VertexLight l3 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX3,
                                          worldY3, worldZ3);
        VertexLight l4 = buildVertexLight(buildData, direction, true, rawLight, tint, worldX4,
                                          worldY4, worldZ4);
        addFace4UVSmooth(bucket.vertices, bucket.rawLights, bucket.atlasRects, bucket.shades,
                         bucket.tints, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, u0, v0, u1,
                         v1, l1, l2, l3, l4, shadeMul, tint, atlasRect);
    };

    {
        std::vector<MaskCell> mask;
        mask.resize((size_t) Chunk::SIZE_Z * (size_t) Chunk::SIZE_Y);

        for (int x = 0; x <= Chunk::SIZE_X; x++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int z = 0; z < Chunk::SIZE_Z; z++)
                {
                    bool a = isSolidLevel(buildData, BlockPos(x - 1, y, z));
                    bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                    MaskCell &cell = mask[(size_t) z + (size_t) Chunk::SIZE_Z * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.rawLight  = 0;
                    cell.tint      = 0xFFFFFF;

                    if (a && !b && x > 0)
                    {
                        Block *block     = Block::byId(chunk->getBlockId(x - 1, y, z));
                        Texture *texture = block ? block->getTexture(Direction::EAST) : nullptr;
                        if (texture)
                        {
                            cell.filled    = true;
                            cell.texture   = texture;
                            cell.atlasRect = block->getAtlasUVRect(Direction::EAST);
                            cell.rawLight =
                                    sampleLightKey(buildData, baseX + x, baseY + y, baseZ + z);
                            cell.tint = block->resolveTint(Direction::EAST, level, chunk, x - 1, z);
                            if (grassSideOverlay && block == grassBlock)
                            {
                                uint32_t foliageTint =
                                        block->resolveTint(Direction::UP, level, chunk, x - 1, z);
                                cell.tint = packTintMode(foliageTint, TINT_MODE_MASKED);
                            }
                        }
                    }
                }
            }

            greedy2D(mask, Chunk::SIZE_Z, Chunk::SIZE_Y, !smoothLighting,
                     [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                         const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                         float X  = (float) x;
                         float y1 = (float) j;
                         float y2 = (float) (j + rHeight);
                         float z1 = (float) i;
                         float z2 = (float) (i + rWidth);
                         emit(Direction::EAST, texture, atlasRect, rawLight, tint, X, y1, z1, X, y2,
                              z2);
                     });
        }

        for (int x = 0; x <= Chunk::SIZE_X; x++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int z = 0; z < Chunk::SIZE_Z; z++)
                {
                    bool a = isSolidLevel(buildData, BlockPos(x - 1, y, z));
                    bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                    MaskCell &cell = mask[(size_t) z + (size_t) Chunk::SIZE_Z * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                    cell.rawLight  = 0;
                    cell.tint      = 0xFFFFFF;

                    if (b && !a && x < Chunk::SIZE_X)
                    {
                        Block *block     = Block::byId(chunk->getBlockId(x, y, z));
                        Texture *texture = block ? block->getTexture(Direction::WEST) : nullptr;
                        if (texture)
                        {
                            cell.filled    = true;
                            cell.texture   = texture;
                            cell.atlasRect = block->getAtlasUVRect(Direction::WEST);
                            cell.rawLight =
                                    sampleLightKey(buildData, baseX + x - 1, baseY + y, baseZ + z);
                            cell.tint = block->resolveTint(Direction::WEST, level, chunk, x, z);
                            if (grassSideOverlay && block == grassBlock)
                            {
                                uint32_t foliageTint =
                                        block->resolveTint(Direction::UP, level, chunk, x, z);
                                cell.tint = packTintMode(foliageTint, TINT_MODE_MASKED);
                            }
                        }
                    }
                }
            }

            greedy2D(mask, Chunk::SIZE_Z, Chunk::SIZE_Y, !smoothLighting,
                     [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                         const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                         float X  = (float) x;
                         float y1 = (float) j;
                         float y2 = (float) (j + rHeight);
                         float z1 = (float) i;
                         float z2 = (float) (i + rWidth);
                         emit(Direction::WEST, texture, atlasRect, rawLight, tint, X, y1, z1, X, y2,
                              z2);
                     });
        }
    }

    {
        std::vector<MaskCell> mask;
        mask.resize((size_t) Chunk::SIZE_X * (size_t) Chunk::SIZE_Y);

        for (int z = 0; z <= Chunk::SIZE_Z; z++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int x = 0; x < Chunk::SIZE_X; x++)
                {
                    bool a = isSolidLevel(buildData, BlockPos(x, y, z - 1));
                    bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                    cell.rawLight  = 0;
                    cell.tint      = 0xFFFFFF;

                    if (a && !b && z > 0)
                    {
                        Block *block     = Block::byId(chunk->getBlockId(x, y, z - 1));
                        Texture *texture = block ? block->getTexture(Direction::SOUTH) : nullptr;
                        if (texture)
                        {
                            cell.filled    = true;
                            cell.texture   = texture;
                            cell.atlasRect = block->getAtlasUVRect(Direction::SOUTH);
                            cell.rawLight =
                                    sampleLightKey(buildData, baseX + x, baseY + y, baseZ + z);
                            cell.tint =
                                    block->resolveTint(Direction::SOUTH, level, chunk, x, z - 1);
                            if (grassSideOverlay && block == grassBlock)
                            {
                                uint32_t foliageTint =
                                        block->resolveTint(Direction::UP, level, chunk, x, z - 1);
                                cell.tint = packTintMode(foliageTint, TINT_MODE_MASKED);
                            }
                        }
                    }
                }
            }

            greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Y, !smoothLighting,
                     [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                         const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                         float Z  = (float) z;
                         float x1 = (float) i;
                         float x2 = (float) (i + rWidth);
                         float y1 = (float) j;
                         float y2 = (float) (j + rHeight);
                         emit(Direction::SOUTH, texture, atlasRect, rawLight, tint, x1, y1, Z, x2,
                              y2, Z);
                     });
        }

        for (int z = 0; z <= Chunk::SIZE_Z; z++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int x = 0; x < Chunk::SIZE_X; x++)
                {
                    bool a = isSolidLevel(buildData, BlockPos(x, y, z - 1));
                    bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                    cell.rawLight  = 0;
                    cell.tint      = 0xFFFFFF;

                    if (b && !a && z < Chunk::SIZE_Z)
                    {
                        Block *block     = Block::byId(chunk->getBlockId(x, y, z));
                        Texture *texture = block ? block->getTexture(Direction::NORTH) : nullptr;
                        if (texture)
                        {
                            cell.filled    = true;
                            cell.texture   = texture;
                            cell.atlasRect = block->getAtlasUVRect(Direction::NORTH);
                            cell.rawLight =
                                    sampleLightKey(buildData, baseX + x, baseY + y, baseZ + z - 1);
                            cell.tint = block->resolveTint(Direction::NORTH, level, chunk, x, z);
                            if (grassSideOverlay && block == grassBlock)
                            {
                                uint32_t foliageTint =
                                        block->resolveTint(Direction::UP, level, chunk, x, z);
                                cell.tint = packTintMode(foliageTint, TINT_MODE_MASKED);
                            }
                        }
                    }
                }
            }
            {

                greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Y, !smoothLighting,
                         [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                             const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                             float Z  = (float) z;
                             float x1 = (float) i;
                             float x2 = (float) (i + rWidth);
                             float y1 = (float) j;
                             float y2 = (float) (j + rHeight);
                             emit(Direction::NORTH, texture, atlasRect, rawLight, tint, x1, y1, Z,
                                  x2, y2, Z);
                         });
            }
        }

        {
            std::vector<MaskCell> mask;
            mask.resize((size_t) Chunk::SIZE_X * (size_t) Chunk::SIZE_Z);

            for (int y = 0; y <= Chunk::SIZE_Y; y++)
            {
                for (int z = 0; z < Chunk::SIZE_Z; z++)
                {
                    for (int x = 0; x < Chunk::SIZE_X; x++)
                    {
                        bool a = isSolidLevel(buildData, BlockPos(x, y - 1, z));
                        bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                        MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) z];
                        cell.filled    = false;
                        cell.texture   = nullptr;
                        cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                        cell.rawLight  = 0;
                        cell.tint      = 0xFFFFFF;

                        if (a && !b && y > 0)
                        {
                            Block *block     = Block::byId(chunk->getBlockId(x, y - 1, z));
                            Texture *texture = block ? block->getTexture(Direction::UP) : nullptr;
                            if (texture)
                            {
                                cell.filled    = true;
                                cell.texture   = texture;
                                cell.atlasRect = block->getAtlasUVRect(Direction::UP);
                                cell.rawLight =
                                        sampleLightKey(buildData, baseX + x, baseY + y, baseZ + z);
                                cell.tint = block->resolveTint(Direction::UP, level, chunk, x, z);
                            }
                        }
                    }
                }

                greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Z, !smoothLighting,
                         [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                             const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                             float Y  = (float) y;
                             float x1 = (float) i;
                             float x2 = (float) (i + rWidth);
                             float z1 = (float) j;
                             float z2 = (float) (j + rHeight);
                             emit(Direction::UP, texture, atlasRect, rawLight, tint, x1, Y, z1, x2,
                                  Y, z2);
                         });
            }

            for (int y = 0; y <= Chunk::SIZE_Y; y++)
            {
                for (int z = 0; z < Chunk::SIZE_Z; z++)
                {
                    for (int x = 0; x < Chunk::SIZE_X; x++)
                    {
                        bool a = isSolidLevel(buildData, BlockPos(x, y - 1, z));
                        bool b = isSolidLevel(buildData, BlockPos(x, y, z));

                        MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) z];
                        cell.filled    = false;
                        cell.texture   = nullptr;
                        cell.atlasRect = {0.0f, 0.0f, 1.0f, 1.0f};
                        cell.rawLight  = 0;
                        cell.tint      = 0xFFFFFF;

                        if (b && !a && y < Chunk::SIZE_Y && y > 0)
                        {
                            Block *block     = Block::byId(chunk->getBlockId(x, y, z));
                            Texture *texture = block ? block->getTexture(Direction::DOWN) : nullptr;
                            if (texture)
                            {
                                cell.filled    = true;
                                cell.texture   = texture;
                                cell.atlasRect = block->getAtlasUVRect(Direction::DOWN);
                                cell.rawLight  = sampleLightKey(buildData, baseX + x, baseY + y - 1,
                                                                baseZ + z);
                                cell.tint = block->resolveTint(Direction::DOWN, level, chunk, x, z);
                            }
                        }
                    }
                }

                greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Z, !smoothLighting,
                         [&](int i, int j, int rWidth, int rHeight, Texture *texture,
                             const Block::UVRect &atlasRect, uint16_t rawLight, uint32_t tint) {
                             float Y  = (float) y;
                             float x1 = (float) i;
                             float x2 = (float) (i + rWidth);
                             float z1 = (float) j;
                             float z2 = (float) (j + rHeight);
                             emit(Direction::DOWN, texture, atlasRect, rawLight, tint, x1, Y, z1,
                                  x2, Y, z2);
                         });
            }
        }

        for (int x = 0; x < Chunk::SIZE_X; x++)
        {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
            {
                for (int z = 0; z < Chunk::SIZE_Z; z++)
                {
                    Block *block = Block::byId(chunk->getBlockId(x, y, z));
                    if (!block)
                    {
                        continue;
                    }

                    Block::RenderShape renderType = block->getRenderShape();
                    if (renderType != Block::RenderShape::CROSS &&
                        renderType != Block::RenderShape::TORCH)
                    {
                        continue;
                    }

                    Texture *texture = block->getTexture(Direction::UP);
                    if (!texture)
                        continue;

                    Block::UVRect atlasUp = block->getAtlasUVRect(Direction::UP);

                    float wx = (float) x;
                    float wy = (float) y;
                    float wz = (float) z;

                    uint16_t rawLight = sampleLightKey(buildData, baseX + x, baseY + y, baseZ + z);
                    uint32_t tint     = 0xFFFFFF;

                    if (renderType == Block::RenderShape::CROSS)
                    {
                        emitFace4(Direction::NORTH, texture, atlasUp, rawLight, tint, wx, wy, wz,
                                  wx, wy + 1.0f, wz, wx + 1.0f, wy + 1.0f, wz + 1.0f, wx + 1.0f, wy,
                                  wz + 1.0f);
                        emitFace4(Direction::EAST, texture, atlasUp, rawLight, tint, wx + 1.0f, wy,
                                  wz, wx + 1.0f, wy + 1.0f, wz, wx, wy + 1.0f, wz + 1.0f, wx, wy,
                                  wz + 1.0f);
                    }
                    else
                    {
                        const AABB &bb = block->getAABB();

                        float x1 = wx + bb.getMin().x;
                        float y1 = wy + bb.getMin().y;
                        float z1 = wz + bb.getMin().z;

                        float x2 = wx + bb.getMax().x;
                        float y2 = wy + bb.getMax().y;
                        float z2 = wz + bb.getMax().z;

                        Block::UVRect uvNorth    = block->getUVRect(Direction::NORTH);
                        Block::UVRect uvSouth    = block->getUVRect(Direction::SOUTH);
                        Block::UVRect uvWest     = block->getUVRect(Direction::WEST);
                        Block::UVRect uvEast     = block->getUVRect(Direction::EAST);
                        Block::UVRect uvDown     = block->getUVRect(Direction::DOWN);
                        Block::UVRect uvUp       = block->getUVRect(Direction::UP);
                        Block::UVRect atlasNorth = block->getAtlasUVRect(Direction::NORTH);
                        Block::UVRect atlasSouth = block->getAtlasUVRect(Direction::SOUTH);
                        Block::UVRect atlasWest  = block->getAtlasUVRect(Direction::WEST);
                        Block::UVRect atlasEast  = block->getAtlasUVRect(Direction::EAST);
                        Block::UVRect atlasDown  = block->getAtlasUVRect(Direction::DOWN);

                        Texture *textureUp    = texture;
                        Texture *textureNorth = block->getTexture(Direction::NORTH);
                        if (!textureNorth)
                        {
                            textureNorth = textureUp;
                        }
                        Texture *textureSouth = block->getTexture(Direction::SOUTH);
                        if (!textureSouth)
                        {
                            textureSouth = textureUp;
                        }
                        Texture *textureWest = block->getTexture(Direction::WEST);
                        if (!textureWest)
                        {
                            textureWest = textureUp;
                        }
                        Texture *textureEast = block->getTexture(Direction::EAST);
                        if (!textureEast)
                        {
                            textureEast = textureUp;
                        }
                        Texture *textureDown = block->getTexture(Direction::DOWN);
                        if (!textureDown)
                        {
                            textureDown = textureUp;
                        }

                        Direction *attachmentFace =
                                decodeDirection(chunk->getBlockAttachmentFace(x, y, z));
                        bool wallMounted = block->hasWallMountedTransform() &&
                                           (attachmentFace == Direction::NORTH ||
                                            attachmentFace == Direction::SOUTH ||
                                            attachmentFace == Direction::WEST ||
                                            attachmentFace == Direction::EAST);
                        if (!wallMounted)
                        {
                            if (uvNorth.u0 != uvNorth.u1 && uvNorth.v0 != uvNorth.v1)
                            {
                                emitFixedUV(Direction::NORTH, textureNorth, atlasNorth, rawLight,
                                            tint, x1, y1, z1, x2, y2, z1, uvNorth.u0, uvNorth.v0,
                                            uvNorth.u1, uvNorth.v1);
                            }
                            if (uvSouth.u0 != uvSouth.u1 && uvSouth.v0 != uvSouth.v1)
                            {
                                emitFixedUV(Direction::SOUTH, textureSouth, atlasSouth, rawLight,
                                            tint, x1, y1, z2, x2, y2, z2, uvSouth.u0, uvSouth.v0,
                                            uvSouth.u1, uvSouth.v1);
                            }
                            if (uvWest.u0 != uvWest.u1 && uvWest.v0 != uvWest.v1)
                            {
                                emitFixedUV(Direction::WEST, textureWest, atlasWest, rawLight, tint,
                                            x1, y1, z1, x1, y2, z2, uvWest.u0, uvWest.v0, uvWest.u1,
                                            uvWest.v1);
                            }
                            if (uvEast.u0 != uvEast.u1 && uvEast.v0 != uvEast.v1)
                            {
                                emitFixedUV(Direction::EAST, textureEast, atlasEast, rawLight, tint,
                                            x2, y1, z1, x2, y2, z2, uvEast.u0, uvEast.v0, uvEast.u1,
                                            uvEast.v1);
                            }
                            if (uvDown.u0 != uvDown.u1 && uvDown.v0 != uvDown.v1)
                            {
                                emitFixedUV(Direction::DOWN, textureDown, atlasDown, rawLight, tint,
                                            x1, y1, z1, x2, y1, z2, uvDown.u0, uvDown.v0, uvDown.u1,
                                            uvDown.v1);
                            }
                            if (uvUp.u0 != uvUp.u1 && uvUp.v0 != uvUp.v1)
                            {
                                emitFixedUV(Direction::UP, textureUp, atlasUp, rawLight, tint, x1,
                                            y2, z1, x2, y2, z2, uvUp.u0, uvUp.v0, uvUp.u1, uvUp.v1);
                            }
                        }
                        else
                        {
                            WallMountedVertex v0{x1, y1, z1};
                            WallMountedVertex v1{x2, y1, z1};
                            WallMountedVertex v2{x2, y2, z1};
                            WallMountedVertex v3{x1, y2, z1};
                            WallMountedVertex v4{x1, y1, z2};
                            WallMountedVertex v5{x2, y1, z2};
                            WallMountedVertex v6{x2, y2, z2};
                            WallMountedVertex v7{x1, y2, z2};

                            WallMountedVertex *vertices[8] = {&v0, &v1, &v2, &v3,
                                                              &v4, &v5, &v6, &v7};

                            float angle =
                                    block->getWallMountedTiltDegrees() * (float) M_PI / 180.0f;
                            if (attachmentFace == Direction::SOUTH ||
                                attachmentFace == Direction::WEST)
                            {
                                angle = -angle;
                            }

                            float pivotX = wx + 0.5f;
                            float pivotY = y1;
                            float pivotZ = wz + 0.5f;

                            float c = cosf(angle);
                            float s = sinf(angle);

                            if (attachmentFace == Direction::NORTH ||
                                attachmentFace == Direction::SOUTH)
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    float dy = vertices[i]->y - pivotY;
                                    float dz = vertices[i]->z - pivotZ;
                                    float ny = dy * c - dz * s;
                                    float nz = dy * s + dz * c;

                                    vertices[i]->y = pivotY + ny;
                                    vertices[i]->z = pivotZ + nz;
                                }
                            }
                            else
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    float dx = vertices[i]->x - pivotX;
                                    float dy = vertices[i]->y - pivotY;
                                    float nx = dx * c - dy * s;
                                    float ny = dx * s + dy * c;

                                    vertices[i]->x = pivotX + nx;
                                    vertices[i]->y = pivotY + ny;
                                }
                            }

                            if (attachmentFace == Direction::NORTH)
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    vertices[i]->z -= block->getWallMountedInset();
                                }
                            }
                            else if (attachmentFace == Direction::SOUTH)
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    vertices[i]->z += block->getWallMountedInset();
                                }
                            }
                            else if (attachmentFace == Direction::WEST)
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    vertices[i]->x -= block->getWallMountedInset();
                                }
                            }
                            else if (attachmentFace == Direction::EAST)
                            {
                                for (int i = 0; i < 8; i++)
                                {
                                    vertices[i]->x += block->getWallMountedInset();
                                }
                            }

                            if (uvNorth.u0 != uvNorth.u1 && uvNorth.v0 != uvNorth.v1)
                            {
                                emitFace4FixedUV(Direction::NORTH, textureNorth, atlasNorth,
                                                 rawLight, tint, v0.x, v0.y, v0.z, v3.x, v3.y, v3.z,
                                                 v2.x, v2.y, v2.z, v1.x, v1.y, v1.z, uvNorth.u0,
                                                 uvNorth.v0, uvNorth.u1, uvNorth.v1);
                            }
                            if (uvSouth.u0 != uvSouth.u1 && uvSouth.v0 != uvSouth.v1)
                            {
                                emitFace4FixedUV(Direction::SOUTH, textureSouth, atlasSouth,
                                                 rawLight, tint, v5.x, v5.y, v5.z, v6.x, v6.y, v6.z,
                                                 v7.x, v7.y, v7.z, v4.x, v4.y, v4.z, uvSouth.u0,
                                                 uvSouth.v0, uvSouth.u1, uvSouth.v1);
                            }
                            if (uvWest.u0 != uvWest.u1 && uvWest.v0 != uvWest.v1)
                            {
                                emitFace4FixedUV(Direction::WEST, textureWest, atlasWest, rawLight,
                                                 tint, v4.x, v4.y, v4.z, v7.x, v7.y, v7.z, v3.x,
                                                 v3.y, v3.z, v0.x, v0.y, v0.z, uvWest.u0, uvWest.v0,
                                                 uvWest.u1, uvWest.v1);
                            }
                            if (uvEast.u0 != uvEast.u1 && uvEast.v0 != uvEast.v1)
                            {
                                emitFace4FixedUV(Direction::EAST, textureEast, atlasEast, rawLight,
                                                 tint, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v6.x,
                                                 v6.y, v6.z, v5.x, v5.y, v5.z, uvEast.u0, uvEast.v0,
                                                 uvEast.u1, uvEast.v1);
                            }
                            if (uvDown.u0 != uvDown.u1 && uvDown.v0 != uvDown.v1)
                            {
                                emitFace4FixedUV(Direction::DOWN, textureDown, atlasDown, rawLight,
                                                 tint, v4.x, v4.y, v4.z, v0.x, v0.y, v0.z, v1.x,
                                                 v1.y, v1.z, v5.x, v5.y, v5.z, uvDown.u0, uvDown.v0,
                                                 uvDown.u1, uvDown.v1);
                            }
                            if (uvUp.u0 != uvUp.u1 && uvUp.v0 != uvUp.v1)
                            {
                                emitFace4FixedUV(Direction::UP, textureUp, atlasUp, rawLight, tint,
                                                 v3.x, v3.y, v3.z, v7.x, v7.y, v7.z, v6.x, v6.y,
                                                 v6.z, v2.x, v2.y, v2.z, uvUp.u0, uvUp.v0, uvUp.u1,
                                                 uvUp.v1);
                            }
                        }
                    }
                }
            }
        }

        outMeshes->clear();
        outMeshes->reserve(buckets.size());

        std::unordered_map<Texture *, MeshBucket>::iterator it;
        for (it = buckets.begin(); it != buckets.end(); ++it)
        {
            if (!it->first)
            {
                continue;
            }
            if (it->second.vertices.empty())
            {
                continue;
            }

            MeshBuildResult result;
            result.texture    = it->first;
            result.vertices   = std::move(it->second.vertices);
            result.atlasRects = std::move(it->second.atlasRects);
            result.rawLights  = std::move(it->second.rawLights);
            result.shades     = std::move(it->second.shades);
            result.tints      = std::move(it->second.tints);
            outMeshes->push_back(std::move(result));
        }
    }
}
