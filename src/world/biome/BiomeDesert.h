#pragma once

#include "Biome.h"

class BiomeDesert : public Biome {
public:
    BiomeDesert();

    Block *getTopBlock() const override;
    Block *getFillerBlock() const override;

    int getBaseHeight() const override;
    int getHeightVariation() const override;

    float getTemperature() const override;
    float getHumidity() const override;
};
