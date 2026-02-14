#pragma once

#include <memory>
#include <string>

#include "ModelPartDefinition.h"
#include "ModelPartSkin.h"

class ModelDefinition {
public:
    ModelDefinition();
    ModelDefinition(const std::string &id, const ModelPartSkin &skin);

    const std::string &getId() const;

    void setSkin(const ModelPartSkin &skin);
    const ModelPartSkin &getSkin() const;

    ModelPartDefinition *getRoot();
    const ModelPartDefinition *getRoot() const;

private:
    std::string m_id;
    ModelPartSkin m_skin;
    std::unique_ptr<ModelPartDefinition> m_root;
};
