#include "TextureRepository.h"

TextureRepository::TextureRepository() {}

std::shared_ptr<Texture> TextureRepository::get(const std::string &path) {
    auto it = m_textures.find(path);
    if (it != m_textures.end()) return it->second;

    std::shared_ptr<Texture> texture = std::make_shared<Texture>(path.c_str());
    m_textures[path]                 = texture;
    return texture;
}
