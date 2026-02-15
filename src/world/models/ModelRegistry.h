#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "../../rendering/TextureRepository.h"

class Model;

class ModelRegistry {
public:
    static ModelRegistry *get();
    static void init();

    void registerModel(const std::string &name, std::unique_ptr<Model> model);
    Model *getModel(const std::string &name) const;
    bool hasModel(const std::string &name) const;

    TextureRepository *getTextures();

private:
    ModelRegistry() = default;

    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;
    TextureRepository m_textures;
};
