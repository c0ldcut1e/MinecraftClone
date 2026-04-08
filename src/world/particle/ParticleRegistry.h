#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../rendering/TextureAtlas.h"
#include "../../rendering/TextureRepository.h"
#include "../../utils/math/Vec3.h"
#include "ParticleType.h"

struct ParticleDefinition
{
    ParticleDefinition();
    ParticleDefinition(ParticleType type, const std::string &texturePath, uint32_t color,
                       float size, int lifetime, float gravity, float drag, bool lit,
                       bool collides, const Vec3 &velocity, const Vec3 &velocitySpread);

    ParticleType type;
    std::string texturePath;
    uint32_t color;
    float size;
    int lifetime;
    float gravity;
    float drag;
    bool lit;
    bool collides;
    Vec3 velocity;
    Vec3 velocitySpread;
};

class ParticleRegistry
{
public:
    static void init();
    static void clear();
    static void registerParticle(const ParticleDefinition &definition);

    static bool has(ParticleType type);
    static const ParticleDefinition *get(ParticleType type);

    static void rebuildAtlas(TextureRepository *repository);
    static bool hasAtlasRegion(ParticleType type);
    static TextureAtlas::Region getAtlasRegion(ParticleType type);
    static Texture *getTexture();

private:
    static std::unordered_map<ParticleType, ParticleDefinition, ParticleTypeHash> s_definitions;
    static std::unordered_map<ParticleType, TextureAtlas::Region, ParticleTypeHash> s_regions;
    static TextureAtlas s_atlas;
};
