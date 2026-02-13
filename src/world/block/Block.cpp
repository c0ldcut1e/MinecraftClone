#include "Block.h"
#include "BlockRegistry.h"

Block::Block() : m_name(""), m_solid(false), m_aabb(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, 0.0)), m_lightEmission(0), m_lightR(0), m_lightG(0), m_lightB(0) {}

Block::Block(const std::string &name, bool solid, const std::string &texturePath) : m_name(name), m_solid(solid), m_aabb(Vec3(0.0, 0.0, 0.0), solid ? Vec3(1.0, 1.0, 1.0) : Vec3(0.0, 0.0, 0.0)), m_lightEmission(0), m_lightR(0), m_lightG(0), m_lightB(0) {
    if (!texturePath.empty()) {
        TextureRepository *textureRepo = BlockRegistry::getTextureRepository();
        setTexture(Direction::UP, textureRepo->get(texturePath).get());
        setTexture(Direction::DOWN, textureRepo->get(texturePath).get());
        setTexture(Direction::NORTH, textureRepo->get(texturePath).get());
        setTexture(Direction::SOUTH, textureRepo->get(texturePath).get());
        setTexture(Direction::EAST, textureRepo->get(texturePath).get());
        setTexture(Direction::WEST, textureRepo->get(texturePath).get());
    }
}

Block *Block::byId(uint32_t id) { return BlockRegistry::get()->byId(id); }

Block *Block::byName(const std::string &name) { return byId(BlockRegistry::get()->getId(name)); }

void Block::setTexture(Direction *direction, Texture *texture) { m_textures[direction] = texture; }

Texture *Block::getTexture(Direction *direction) const {
    auto it = m_textures.find(direction);
    if (it == m_textures.end()) return nullptr;
    return it->second;
}

const std::string &Block::getName() const { return m_name; }

bool Block::isSolid() const { return m_solid; }

const AABB &Block::getAABB() const { return m_aabb; }

void Block::setLightEmission(uint8_t value) { m_lightEmission = value; }

uint8_t Block::getLightEmission() const { return m_lightEmission; }

void Block::setLightColor(uint8_t r, uint8_t g, uint8_t b) {
    m_lightR = r;
    m_lightG = g;
    m_lightB = b;
}

void Block::getLightColor(uint8_t &r, uint8_t &g, uint8_t &b) const {
    r = m_lightR;
    g = m_lightG;
    b = m_lightB;
}
