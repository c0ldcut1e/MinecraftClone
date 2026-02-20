#include "Block.h"
#include "BlockRegistry.h"

#include "../../rendering/ColormapManager.h"
#include "../World.h"
#include "../biome/Biome.h"
#include "../chunk/Chunk.h"

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

void Block::setTintColormap(const std::string &colormapName) {
    m_tintColormaps[Direction::UP]    = colormapName;
    m_tintColormaps[Direction::DOWN]  = colormapName;
    m_tintColormaps[Direction::NORTH] = colormapName;
    m_tintColormaps[Direction::SOUTH] = colormapName;
    m_tintColormaps[Direction::EAST]  = colormapName;
    m_tintColormaps[Direction::WEST]  = colormapName;
}

void Block::setTintColormap(Direction *direction, const std::string &colormapName) { m_tintColormaps[direction] = colormapName; }

bool Block::hasTintColormap(Direction *direction) const { return m_tintColormaps.find(direction) != m_tintColormaps.end(); }

const std::string &Block::getTintColormap(Direction *direction) const {
    auto it = m_tintColormaps.find(direction);
    static const std::string empty;
    if (it == m_tintColormaps.end()) return empty;
    return it->second;
}

uint32_t Block::resolveTint(Direction *direction, World *world, const Chunk *chunk, int localX, int localZ) const {
    if (!hasTintColormap(direction)) return 0xFFFFFF;

    static constexpr int RADIUS = 2;
    float totalTemp             = 0.0f;
    float totalHumid            = 0.0f;
    int samples                 = 0;

    for (int dz = -RADIUS; dz <= RADIUS; dz++)
        for (int dx = -RADIUS; dx <= RADIUS; dx++) {
            int lx = localX + dx;
            int lz = localZ + dz;

            int chunkOffsetX = 0;
            int chunkOffsetZ = 0;

            if (lx < 0) {
                chunkOffsetX = -1;
                lx += Chunk::SIZE_X;
            } else if (lx >= Chunk::SIZE_X) {
                chunkOffsetX = 1;
                lx -= Chunk::SIZE_X;
            }

            if (lz < 0) {
                chunkOffsetZ = -1;
                lz += Chunk::SIZE_Z;
            } else if (lz >= Chunk::SIZE_Z) {
                chunkOffsetZ = 1;
                lz -= Chunk::SIZE_Z;
            }

            const Chunk *target = chunk;
            if (chunkOffsetX != 0 || chunkOffsetZ != 0) {
                ChunkPos pos = chunk->getPos();
                pos.x += chunkOffsetX;
                pos.z += chunkOffsetZ;
                target = world->getChunk(pos);
                if (!target) continue;
            }

            Biome *biome = target->getBiomeAt(lx, lz);
            if (!biome) continue;

            totalTemp += biome->getTemperature();
            totalHumid += biome->getHumidity();
            samples++;
        }

    if (samples == 0) return 0xFFFFFF;

    float r, g, b;
    ColormapManager::getInstance()->sampleFoliageColor(getTintColormap(direction), totalTemp / (float) samples, totalHumid / (float) samples, r, g, b);
    return ((uint32_t) (r * 255.0f) << 16) | ((uint32_t) (g * 255.0f) << 8) | (uint32_t) (b * 255.0f);
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
