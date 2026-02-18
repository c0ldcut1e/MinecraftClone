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

    Model *copy() const;

    void rebuild(const ModelDefinition &definition);

    const ModelPartSkin &getSkin() const;

    ModelPart *getRoot();
    ModelPart *findPart(const std::string &name);

    void render(TextureRepository &textures) const;
    void renderPart(const ModelPart *part, int textureWidth, int textureHeight) const;

private:
    static std::unique_ptr<ModelPart> buildPartTree(const ModelPartDefinition &definition);
    static std::unique_ptr<ModelPart> clonePartTree(const ModelPart *part);

private:
    ModelPartSkin m_skin;
    std::unique_ptr<ModelPart> m_root;
};
