#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../rendering/Texture.h"
#include "../../utils/Direction.h"
#include "../../utils/math/AABB.h"

class Chunk;
class World;

class Block {
public:
    Block();
    Block(const std::string &name, bool solid, const std::string &texturePath);

    static Block *byId(uint32_t id);
    static Block *byName(const std::string &name);

    void setTexture(Direction *direction, Texture *texture);
    Texture *getTexture(Direction *direction) const;

    void setTintColormap(const std::string &colormapName);
    void setTintColormap(Direction *direction, const std::string &colormapName);
    bool hasTintColormap(Direction *direction) const;
    const std::string &getTintColormap(Direction *direction) const;

    uint32_t resolveTint(Direction *direction, World *world, const Chunk *chunk, int localX, int localZ) const;

    const std::string &getName() const;
    bool isSolid() const;

    const AABB &getAABB() const;

    void setLightEmission(uint8_t value);
    uint8_t getLightEmission() const;

    void setLightColor(uint8_t r, uint8_t g, uint8_t b);
    void getLightColor(uint8_t &r, uint8_t &g, uint8_t &b) const;

private:
    std::unordered_map<Direction *, Texture *> m_textures;
    std::unordered_map<Direction *, std::string> m_tintColormaps;
    std::string m_name;
    bool m_solid;
    AABB m_aabb;
    uint8_t m_lightEmission;
    uint8_t m_lightR;
    uint8_t m_lightG;
    uint8_t m_lightB;
};
