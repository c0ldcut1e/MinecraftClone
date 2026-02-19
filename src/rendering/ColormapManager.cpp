#include "ColormapManager.h"
#include "Texture.h"

ColormapManager *ColormapManager::getInstance() {
    static ColormapManager s_instance;
    return &s_instance;
}

Texture *ColormapManager::load(const std::string &name, const std::string &path) {
    auto it = m_maps.find(name);
    if (it != m_maps.end()) return it->second;

    Texture *texture = new Texture(path.c_str());
    m_maps[name]     = texture;
    return texture;
}

Texture *ColormapManager::get(const std::string &name) const {
    auto it = m_maps.find(name);
    if (it == m_maps.end()) return nullptr;
    return it->second;
}

void ColormapManager::bind(const std::string &name, uint32_t slot) const {
    Texture *texture = get(name);
    if (texture) texture->bind(slot);
}

void ColormapManager::clear() {
    for (auto &kv : m_maps) delete kv.second;
    m_maps.clear();
}
