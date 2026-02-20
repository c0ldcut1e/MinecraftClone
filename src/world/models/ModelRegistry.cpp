#include "ModelRegistry.h"

#include "../../core/Logger.h"
#include "Model.h"
#include "ModelPartsSkinned.h"

ModelRegistry *ModelRegistry::get() {
    static ModelRegistry instance;
    return &instance;
}

void ModelRegistry::init() {
    ModelRegistry *registry = ModelRegistry::get();

    ModelDefinition steveDef     = ModelPartsSkinned::createSteveClassic("steve", "textures/models/steve.png");
    std::unique_ptr<Model> steve = std::make_unique<Model>(steveDef);
    registry->registerModel("steve", std::move(steve));

    Logger::logInfo("ModelRegistry initialized (models=%d)", (int) registry->m_models.size());
}

void ModelRegistry::registerModel(const std::string &name, std::unique_ptr<Model> model) {
    if (!model) {
        Logger::logWarn("ModelRegistry: tried to register null model '%s'", name.c_str());
        return;
    }

    if (hasModel(name)) Logger::logWarn("ModelRegistry: overwriting model '%s'", name.c_str());

    m_models[name] = std::move(model);
}

Model *ModelRegistry::getModel(const std::string &name) const {
    auto it = m_models.find(name);
    if (it == m_models.end()) return nullptr;
    return it->second.get();
}

bool ModelRegistry::hasModel(const std::string &name) const { return m_models.find(name) != m_models.end(); }

TextureRepository *ModelRegistry::getTextures() { return &m_textures; }
