#pragma once

#include "Biome.h"

class BiomePlains : public Biome {
public:
    BiomePlains();

    Block *getTopBlock() const override;
    Block *getFillerBlock() const override;

    int getBaseHeight() const override;
    int getHeightVariation() const override;

    float getTemperature() const override;
    float getHumidity() const override;
};
