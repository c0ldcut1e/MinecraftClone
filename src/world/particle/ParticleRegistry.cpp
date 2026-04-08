#include "ParticleRegistry.h"

#include <algorithm>

std::unordered_map<ParticleType, ParticleDefinition, ParticleTypeHash>
        ParticleRegistry::s_definitions;
std::unordered_map<ParticleType, TextureAtlas::Region, ParticleTypeHash>
        ParticleRegistry::s_regions;
TextureAtlas ParticleRegistry::s_atlas;

ParticleDefinition::ParticleDefinition()
    : texturePath(), color(0xFFFFFFFF), size(0.1f), lifetime(20), gravity(0.0f), drag(0.0f),
      lit(false), collides(false), velocity(), velocitySpread()
{}

ParticleDefinition::ParticleDefinition(ParticleType type, const std::string &texturePath,
                                       uint32_t color, float size, int lifetime, float gravity,
                                       float drag, bool lit, bool collides, const Vec3 &velocity,
                                       const Vec3 &velocitySpread)
    : type(type), texturePath(texturePath), color(color), size(size), lifetime(lifetime),
      gravity(gravity), drag(drag), lit(lit), collides(collides), velocity(velocity),
      velocitySpread(velocitySpread)
{}

void ParticleRegistry::init()
{
    clear();

    registerParticle(ParticleDefinition(ParticleType::SPLASH, "textures/particle/splash_0.png",
                                        0xFFFFFFFF, 0.1f, 12, -32.0f, 0.0f, false, false,
                                        Vec3(0.0, 0.0, 0.0), Vec3(0.02, 0.02, 0.02)));
}

void ParticleRegistry::clear()
{
    s_definitions.clear();
    s_regions.clear();
    s_atlas.build(nullptr, std::vector<std::string>());
}

void ParticleRegistry::registerParticle(const ParticleDefinition &definition)
{
    s_definitions[definition.type] = definition;
}

bool ParticleRegistry::has(ParticleType type)
{
    return s_definitions.find(type) != s_definitions.end();
}

const ParticleDefinition *ParticleRegistry::get(ParticleType type)
{
    std::unordered_map<ParticleType, ParticleDefinition, ParticleTypeHash>::const_iterator it =
            s_definitions.find(type);
    if (it == s_definitions.end())
    {
        return nullptr;
    }

    return &it->second;
}

void ParticleRegistry::rebuildAtlas(TextureRepository *repository)
{
    s_regions.clear();

    std::vector<std::pair<ParticleType, std::string>> entries;
    entries.reserve(s_definitions.size());

    std::unordered_map<ParticleType, ParticleDefinition, ParticleTypeHash>::const_iterator it;
    for (it = s_definitions.begin(); it != s_definitions.end(); ++it)
    {
        if (!it->second.texturePath.empty())
        {
            entries.emplace_back(it->first, it->second.texturePath);
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const std::pair<ParticleType, std::string> &a,
                 const std::pair<ParticleType, std::string> &b) {
                  return (uint16_t) a.first < (uint16_t) b.first;
              });

    std::vector<std::string> paths;
    paths.reserve(entries.size());

    for (const std::pair<ParticleType, std::string> &entry : entries)
    {
        if (std::find(paths.begin(), paths.end(), entry.second) == paths.end())
        {
            paths.push_back(entry.second);
        }
    }

    s_atlas.build(repository, paths);

    for (const std::pair<ParticleType, std::string> &entry : entries)
    {
        if (s_atlas.has(entry.second))
        {
            s_regions[entry.first] = s_atlas.get(entry.second);
        }
    }
}

bool ParticleRegistry::hasAtlasRegion(ParticleType type)
{
    return s_regions.find(type) != s_regions.end();
}

TextureAtlas::Region ParticleRegistry::getAtlasRegion(ParticleType type)
{
    std::unordered_map<ParticleType, TextureAtlas::Region, ParticleTypeHash>::const_iterator it =
            s_regions.find(type);
    if (it == s_regions.end())
    {
        return TextureAtlas::Region{0.0f, 0.0f, 1.0f, 1.0f};
    }

    return it->second;
}

Texture *ParticleRegistry::getTexture() { return s_atlas.getTexture(); }
