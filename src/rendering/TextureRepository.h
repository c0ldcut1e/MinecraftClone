#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Texture.h"

class TextureRepository {
public:
    TextureRepository();

    std::shared_ptr<Texture> get(const std::string &path);

private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
};
