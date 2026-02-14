#pragma once

#include <memory>
#include <string>

#include "../../rendering/TextureRepository.h"
#include "ModelDefinition.h"
#include "ModelPart.h"
#include "ModelPartSkin.h"

class Model {
public:
    Model();
    explicit Model(const ModelDefinition &definition);

    void rebuild(const ModelDefinition &definition);

    const ModelPartSkin &getSkin() const;

    ModelPart *getRoot();
    ModelPart *findPart(const std::string &name);

    void draw(TextureRepository &textures) const;

private:
    static std::unique_ptr<ModelPart> buildPartTree(const ModelPartDefinition &definition);

private:
    ModelPartSkin m_skin;
    std::unique_ptr<ModelPart> m_root;
};
