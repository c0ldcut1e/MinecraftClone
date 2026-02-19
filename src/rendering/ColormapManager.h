#pragma once

#include <string>
#include <unordered_map>

#include "Texture.h"

class ColormapManager {
public:
    static ColormapManager *getInstance();

    Texture *load(const std::string &name, const std::string &path);
    Texture *get(const std::string &name) const;

    void bind(const std::string &name, uint32_t slot) const;
    void clear();

private:
    ColormapManager()                                   = default;
    ~ColormapManager()                                  = default;
    ColormapManager(const ColormapManager &)            = delete;
    ColormapManager &operator=(const ColormapManager &) = delete;

    std::unordered_map<std::string, Texture *> m_maps;
};
