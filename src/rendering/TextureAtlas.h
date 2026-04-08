#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Texture.h"
#include "TextureRepository.h"

class TextureAtlas
{
public:
    struct Region
    {
        float u0;
        float v0;
        float u1;
        float v1;
    };

    TextureAtlas();

    void build(TextureRepository *repository, const std::vector<std::string> &paths);

    bool has(const std::string &path) const;
    Region get(const std::string &path) const;
    Texture *getTexture() const;

private:
    std::unordered_map<std::string, Region> m_regions;
    std::shared_ptr<Texture> m_texture;
};
