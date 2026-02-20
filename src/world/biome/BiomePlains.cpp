#include "BiomePlains.h"

#include "../block/Block.h"

BiomePlains::BiomePlains() : Biome("plains") {}

Block *BiomePlains::getTopBlock() const { return Block::byName("grass"); }

Block *BiomePlains::getFillerBlock() const { return Block::byName("dirt"); }

int BiomePlains::getBaseHeight() const { return 64; }

int BiomePlains::getHeightVariation() const { return 10; }

float BiomePlains::getTemperature() const { return 1.0f; }

float BiomePlains::getHumidity() const { return 0.3f; }
