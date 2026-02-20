#include "BiomeDesert.h"

#include "../block/Block.h"

BiomeDesert::BiomeDesert() : Biome("desert") {}

Block *BiomeDesert::getTopBlock() const { return Block::byName("sand"); }

Block *BiomeDesert::getFillerBlock() const { return Block::byName("sand"); }

int BiomeDesert::getBaseHeight() const { return 62; }

int BiomeDesert::getHeightVariation() const { return 8; }

float BiomeDesert::getTemperature() const { return 0.8f; }

float BiomeDesert::getHumidity() const { return 0.0f; }
