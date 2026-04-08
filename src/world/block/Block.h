#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../rendering/Texture.h"
#include "../../utils/AABB.h"
#include "../../utils/Direction.h"
#include "BlockPos.h"

class Chunk;
class Level;

class Block
{
public:
    enum class RenderShape : uint8_t
    {
        CUBE  = 0,
        CROSS = 1,
        TORCH = 2,
    };

    struct UVRect
    {
        float u0;
        float v0;
        float u1;
        float v1;
    };

    Block();
    Block(const std::string &name, bool solid, const std::string &texturePath);
    virtual ~Block() = default;

    static Block *byId(uint32_t id);
    static Block *byName(const std::string &name);

    virtual void onPlace(Level *level, const BlockPos &pos);
    virtual void onBreak(Level *level, const BlockPos &pos);
    virtual void tick(Level *level, const BlockPos &pos);

    void setTexture(Direction *direction, Texture *texture);
    Texture *getTexture(Direction *direction) const;
    void setTexturePath(Direction *direction, const std::string &path);
    const std::string &getTexturePath(Direction *direction) const;

    void setTintColormap(const std::string &colormapName);
    void setTintColormap(Direction *direction, const std::string &colormapName);
    bool hasTintColormap(Direction *direction) const;
    const std::string &getTintColormap(Direction *direction) const;

    uint32_t resolveTint(Direction *direction, Level *level, const Chunk *chunk, int localX,
                         int localZ) const;

    const std::string &getName() const;

    bool isSolid() const;
    void setSelectable(bool selectable);
    bool isSelectable() const;

    void setAABB(const AABB &aabb);
    const AABB &getAABB() const;
    void setInteractionAABB(const AABB &aabb);
    const AABB &getInteractionAABB() const;
    void setInteractionAttachmentOffset(float offset);
    float getInteractionAttachmentOffset() const;
    AABB getPlacedAABB(const BlockPos &pos, Direction *attachmentFace) const;

    void setLightEmission(uint8_t value);
    uint8_t getLightEmission() const;

    void setLightColor(uint8_t r, uint8_t g, uint8_t b);
    void getLightColor(uint8_t *r, uint8_t *g, uint8_t *b) const;

    void setRenderShape(RenderShape type);
    RenderShape getRenderShape() const;

    void setUVRect(Direction *direction, float u0, float v0, float u1, float v1);
    UVRect getUVRect(Direction *direction) const;
    void setAtlasUVRect(Direction *direction, float u0, float v0, float u1, float v1);
    UVRect getAtlasUVRect(Direction *direction) const;

    void setWallMountedTransform(float tiltDegrees, float wallInset);
    bool hasWallMountedTransform() const;
    float getWallMountedTiltDegrees() const;
    float getWallMountedInset() const;

protected:
    std::unordered_map<Direction *, Texture *> m_textures;
    std::unordered_map<Direction *, std::string> m_texturePaths;
    std::unordered_map<Direction *, std::string> m_tintColormaps;
    std::string m_name;
    bool m_solid;
    bool m_selectable;
    AABB m_aabb;
    AABB m_interactionAabb;
    bool m_hasInteractionAabb;
    float m_interactionAttachmentOffset;
    uint8_t m_lightEmission;
    uint8_t m_lightR;
    uint8_t m_lightG;
    uint8_t m_lightB;
    RenderShape m_renderShape;
    std::unordered_map<Direction *, UVRect> m_uvRects;
    std::unordered_map<Direction *, UVRect> m_atlasUvRects;
    bool m_hasWallMountedTransform;
    float m_wallMountedTiltDegrees;
    float m_wallMountedInset;
};
