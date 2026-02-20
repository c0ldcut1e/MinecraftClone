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

void ColormapManager::sampleFoliageColor(const std::string &name, float temperature, float humidity, float &r, float &g, float &b) const {
    Texture *colormap = get(name);
    if (!colormap) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        return;
    }

    if (temperature < 0.0f) temperature = 0.0f;
    if (temperature > 1.0f) temperature = 1.0f;
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 1.0f) humidity = 1.0f;

    float adjustedHumidity = humidity * temperature;

    int pixelX = (int) ((1.0f - temperature) * (float) (colormap->getPixelWidth() - 1));
    int pixelY = (int) ((1.0f - adjustedHumidity) * (float) (colormap->getPixelHeight() - 1));
    colormap->samplePixel(pixelX, pixelY, r, g, b);
}

void ColormapManager::clear() {
    for (auto &kv : m_maps) delete kv.second;
    m_maps.clear();
}
