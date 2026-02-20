#pragma once

#include "../../utils/MappedRegistry.h"
#include "Biome.h"

class BiomeRegistry : public MappedRegistry<Biome *> {
public:
    static BiomeRegistry *get();
    static void init();
};
