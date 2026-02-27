#include "Block.h"
#include "BlockRegistry.h"

#include "../../rendering/ColormapManager.h"
#include "../World.h"
#include "../biome/Biome.h"
#include "../chunk/Chunk.h"

Block::Block()
    : m_name(""), m_solid(false), m_aabb(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, 0.0)), m_interactionAabb(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, 0.0)), m_hasInteractionAabb(false), m_interactionAttachmentOffset(0.0f), m_lightEmission(0), m_lightR(0), m_lightG(0),
      m_lightB(0), m_renderShape(RenderShape::CUBE), m_hasWallMountedTransform(false),
      m_wallMountedTiltDegrees(0.0f), m_wallMountedInset(0.0f) {}

Block::Block(const std::string &name, bool solid, const std::string &texturePath)
    : m_name(name), m_solid(solid), m_aabb(Vec3(0.0, 0.0, 0.0), solid ? Vec3(1.0, 1.0, 1.0) : Vec3(0.0, 0.0, 0.0)), m_lightEmission(0), m_lightR(0), m_lightG(0), m_lightB(0), m_renderShape(RenderShape::CUBE),
      m_hasWallMountedTransform(false), m_wallMountedTiltDegrees(0.0f), m_wallMountedInset(0.0f) {
    m_interactionAabb  = m_aabb;
    m_hasInteractionAabb = false;
    m_interactionAttachmentOffset = 0.0f;
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

void Block::onPlace(World *world, const BlockPos &pos) {
    (void) world;
    (void) pos;
}

void Block::onBreak(World *world, const BlockPos &pos) {
    (void) world;
    (void) pos;
}

void Block::tick(World *world, const BlockPos &pos) {
    (void) world;
    (void) pos;
}

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
    ColormapManager::getInstance()->sampleFoliageColor(getTintColormap(direction), totalTemp / (float) samples, totalHumid / (float) samples, &r, &g, &b);
    return ((uint32_t) (r * 255.0f) << 16) | ((uint32_t) (g * 255.0f) << 8) | (uint32_t) (b * 255.0f);
}

const std::string &Block::getName() const { return m_name; }

bool Block::isSolid() const { return m_solid; }

void Block::setAABB(const AABB &aabb) { m_aabb = aabb; }

const AABB &Block::getAABB() const { return m_aabb; }

void Block::setInteractionAABB(const AABB &aabb) {
    m_interactionAabb  = aabb;
    m_hasInteractionAabb = true;
}

const AABB &Block::getInteractionAABB() const {
    if (m_hasInteractionAabb) return m_interactionAabb;
    return m_aabb;
}

void Block::setInteractionAttachmentOffset(float offset) { m_interactionAttachmentOffset = offset; }

float Block::getInteractionAttachmentOffset() const { return m_interactionAttachmentOffset; }

AABB Block::getPlacedAABB(const BlockPos &pos, Direction *attachmentFace) const {
    AABB box = getInteractionAABB().translated(Vec3(pos.x, pos.y, pos.z));
    if (!m_hasWallMountedTransform) return box;

    if (attachmentFace == Direction::NORTH) return box.translated(Vec3(0.0, 0.0, -(m_wallMountedInset + m_interactionAttachmentOffset)));
    if (attachmentFace == Direction::SOUTH) return box.translated(Vec3(0.0, 0.0, m_wallMountedInset + m_interactionAttachmentOffset));
    if (attachmentFace == Direction::WEST) return box.translated(Vec3(-(m_wallMountedInset + m_interactionAttachmentOffset), 0.0, 0.0));
    if (attachmentFace == Direction::EAST) return box.translated(Vec3(m_wallMountedInset + m_interactionAttachmentOffset, 0.0, 0.0));

    return box;
}

void Block::setLightEmission(uint8_t value) { m_lightEmission = value; }

uint8_t Block::getLightEmission() const { return m_lightEmission; }

void Block::setLightColor(uint8_t r, uint8_t g, uint8_t b) {
    m_lightR = r;
    m_lightG = g;
    m_lightB = b;
}

void Block::getLightColor(uint8_t *r, uint8_t *g, uint8_t *b) const {
    *r = m_lightR;
    *g = m_lightG;
    *b = m_lightB;
}

void Block::setRenderShape(RenderShape type) { m_renderShape = type; }

Block::RenderShape Block::getRenderShape() const { return m_renderShape; }

void Block::setUVRect(Direction *direction, float u0, float v0, float u1, float v1) { m_uvRects[direction] = {u0, v0, u1, v1}; }

Block::UVRect Block::getUVRect(Direction *direction) const {
    auto it = m_uvRects.find(direction);
    if (it == m_uvRects.end()) return {0.0f, 0.0f, 1.0f, 1.0f};
    return it->second;
}

void Block::setWallMountedTransform(float tiltDegrees, float wallInset) {
    m_hasWallMountedTransform = true;
    m_wallMountedTiltDegrees  = tiltDegrees;
    m_wallMountedInset        = wallInset;
}

bool Block::hasWallMountedTransform() const { return m_hasWallMountedTransform; }

float Block::getWallMountedTiltDegrees() const { return m_wallMountedTiltDegrees; }

float Block::getWallMountedInset() const { return m_wallMountedInset; }
