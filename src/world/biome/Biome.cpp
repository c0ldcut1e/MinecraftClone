#include "Biome.h"
#include "BiomeRegistry.h"

Biome::Biome(const std::string &name) : m_name(name) {}

Biome::~Biome() {}

Biome *Biome::byId(uint32_t id) { return *BiomeRegistry::get()->byId(id); }

Biome *Biome::byName(const std::string &name) { return byId(BiomeRegistry::get()->getId(name)); }

const std::string &Biome::getName() const { return m_name; }
