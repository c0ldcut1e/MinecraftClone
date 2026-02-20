#include "BiomeRegistry.h"

#include "BiomeDesert.h"
#include "BiomePlains.h"

BiomeRegistry *BiomeRegistry::get() {
    static BiomeRegistry instance;
    return &instance;
}

void BiomeRegistry::init() {
    BiomeRegistry *registry = get();
    registry->registerValue("plains", new BiomePlains());
    registry->registerValue("desert", new BiomeDesert());
}
