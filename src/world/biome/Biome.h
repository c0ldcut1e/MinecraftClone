#pragma once

#include <cstdint>
#include <string>

class Block;

class Biome {
public:
    explicit Biome(const std::string &name);
    virtual ~Biome();

    static Biome *byId(uint32_t id);
    static Biome *byName(const std::string &name);

    const std::string &getName() const;

    virtual Block *getTopBlock() const    = 0;
    virtual Block *getFillerBlock() const = 0;

    virtual int getBaseHeight() const      = 0;
    virtual int getHeightVariation() const = 0;

    virtual float getTemperature() const = 0;
    virtual float getHumidity() const    = 0;

private:
    std::string m_name;
};
