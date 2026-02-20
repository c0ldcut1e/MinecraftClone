#include "Model.h"

#include <glad/glad.h>

#include "../../core/Logger.h"
#include "../../rendering/GlStateManager.h"
#include "../../rendering/ImmediateRenderer.h"

Model::Model() : m_skin(), m_root(std::make_unique<ModelPart>("root")) {}

Model::Model(const ModelDefinition &definition) : m_skin(), m_root(std::make_unique<ModelPart>("root")) { rebuild(definition); }

Model *Model::copy() const {
    Model *model  = new Model();
    model->m_skin = m_skin;

    if (m_root) model->m_root = clonePartTree(m_root.get());

    return model;
}

void Model::rebuild(const ModelDefinition &definition) {
    m_skin = definition.getSkin();
    m_root = buildPartTree(*definition.getRoot());
}

const ModelPartSkin &Model::getSkin() const { return m_skin; }

ModelPart *Model::getRoot() { return m_root.get(); }

ModelPart *Model::findPart(const std::string &name) {
    if (!m_root) return nullptr;
    if (m_root->getName() == name) return m_root.get();

    std::vector<ModelPart *> stack;
    stack.push_back(m_root.get());

    while (!stack.empty()) {
        ModelPart *part = stack.back();
        stack.pop_back();

        if (part->getName() == name) return part;

        for (const auto &kv : part->getChildren()) stack.push_back(kv.second.get());
    }

    return nullptr;
}

void Model::render(TextureRepository &textures) const {
    std::shared_ptr<Texture> texture = textures.get(m_skin.getTexturePath());
    if (!texture.get()) {
        Logger::logError("Model render texture missing");
        return;
    }

    ImmediateRenderer::getForWorld()->bindTexture(texture.get());

    int textureWidth  = m_skin.getTextureWidth();
    int textureHeight = m_skin.getTextureHeight();

    if (!m_root) {
        Logger::logError("Model render root missing");
        ImmediateRenderer::getForWorld()->unbindTexture();
        return;
    }

    renderPart(m_root.get(), textureWidth, textureHeight);

    ImmediateRenderer::getForWorld()->unbindTexture();
}

void Model::renderPart(const ModelPart *part, int textureWidth, int textureHeight) const {
    GlStateManager::pushMatrix();

    GlStateManager::translatef((float) part->getPosition().x, (float) part->getPosition().y, (float) part->getPosition().z);
    GlStateManager::translatef((float) part->getPivot().x, (float) part->getPivot().y, (float) part->getPivot().z);
    GlStateManager::rotatef((float) part->getRotation().z, 0.0f, 0.0f, 1.0f);
    GlStateManager::rotatef((float) part->getRotation().y, 0.0f, 1.0f, 0.0f);
    GlStateManager::rotatef((float) part->getRotation().x, 1.0f, 0.0f, 0.0f);
    GlStateManager::translatef((float) -part->getPivot().x, (float) -part->getPivot().y, (float) -part->getPivot().z);

    for (const ModelPart::Cube &cube : part->getCubes()) {
        ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
        renderer->begin(GL_TRIANGLES);
        renderer->color(0xFFFFFFFF);

        float x0 = cube.min.x;
        float y0 = cube.min.y;
        float z0 = cube.min.z;
        float x1 = cube.max.x;
        float y1 = cube.max.y;
        float z1 = cube.max.z;

        float lu0 = (float) cube.uv.left.x / textureWidth;
        float lv0 = (float) cube.uv.left.y / textureHeight;
        float lu1 = (float) (cube.uv.left.x + cube.uv.left.width) / textureWidth;
        float lv1 = (float) (cube.uv.left.y + cube.uv.left.height) / textureHeight;

        float ru0 = (float) cube.uv.right.x / textureWidth;
        float rv0 = (float) cube.uv.right.y / textureHeight;
        float ru1 = (float) (cube.uv.right.x + cube.uv.right.width) / textureWidth;
        float rv1 = (float) (cube.uv.right.y + cube.uv.right.height) / textureHeight;

        float fu0 = (float) cube.uv.front.x / textureWidth;
        float fv0 = (float) cube.uv.front.y / textureHeight;
        float fu1 = (float) (cube.uv.front.x + cube.uv.front.width) / textureWidth;
        float fv1 = (float) (cube.uv.front.y + cube.uv.front.height) / textureHeight;

        float bu0 = (float) cube.uv.back.x / textureWidth;
        float bv0 = (float) cube.uv.back.y / textureHeight;
        float bu1 = (float) (cube.uv.back.x + cube.uv.back.width) / textureWidth;
        float bv1 = (float) (cube.uv.back.y + cube.uv.back.height) / textureHeight;

        float tu0 = (float) cube.uv.top.x / textureWidth;
        float tv0 = (float) cube.uv.top.y / textureHeight;
        float tu1 = (float) (cube.uv.top.x + cube.uv.top.width) / textureWidth;
        float tv1 = (float) (cube.uv.top.y + cube.uv.top.height) / textureHeight;

        float du0 = (float) cube.uv.bottom.x / textureWidth;
        float dv0 = (float) cube.uv.bottom.y / textureHeight;
        float du1 = (float) (cube.uv.bottom.x + cube.uv.bottom.width) / textureWidth;
        float dv1 = (float) (cube.uv.bottom.y + cube.uv.bottom.height) / textureHeight;

        renderer->vertexUV(x0, y0, z1, fu0, fv1);
        renderer->vertexUV(x1, y0, z1, fu1, fv1);
        renderer->vertexUV(x1, y1, z1, fu1, fv0);
        renderer->vertexUV(x0, y0, z1, fu0, fv1);
        renderer->vertexUV(x1, y1, z1, fu1, fv0);
        renderer->vertexUV(x0, y1, z1, fu0, fv0);

        renderer->vertexUV(x1, y0, z0, bu0, bv1);
        renderer->vertexUV(x0, y0, z0, bu1, bv1);
        renderer->vertexUV(x0, y1, z0, bu1, bv0);
        renderer->vertexUV(x1, y0, z0, bu0, bv1);
        renderer->vertexUV(x0, y1, z0, bu1, bv0);
        renderer->vertexUV(x1, y1, z0, bu0, bv0);

        renderer->vertexUV(x0, y0, z0, lu0, lv1);
        renderer->vertexUV(x0, y0, z1, lu1, lv1);
        renderer->vertexUV(x0, y1, z1, lu1, lv0);
        renderer->vertexUV(x0, y0, z0, lu0, lv1);
        renderer->vertexUV(x0, y1, z1, lu1, lv0);
        renderer->vertexUV(x0, y1, z0, lu0, lv0);

        renderer->vertexUV(x1, y0, z1, ru0, rv1);
        renderer->vertexUV(x1, y0, z0, ru1, rv1);
        renderer->vertexUV(x1, y1, z0, ru1, rv0);
        renderer->vertexUV(x1, y0, z1, ru0, rv1);
        renderer->vertexUV(x1, y1, z0, ru1, rv0);
        renderer->vertexUV(x1, y1, z1, ru0, rv0);

        renderer->vertexUV(x0, y1, z1, tu0, tv1);
        renderer->vertexUV(x1, y1, z1, tu1, tv1);
        renderer->vertexUV(x1, y1, z0, tu1, tv0);
        renderer->vertexUV(x0, y1, z1, tu0, tv1);
        renderer->vertexUV(x1, y1, z0, tu1, tv0);
        renderer->vertexUV(x0, y1, z0, tu0, tv0);

        renderer->vertexUV(x0, y0, z0, du0, dv1);
        renderer->vertexUV(x1, y0, z0, du1, dv1);
        renderer->vertexUV(x1, y0, z1, du1, dv0);
        renderer->vertexUV(x0, y0, z0, du0, dv1);
        renderer->vertexUV(x1, y0, z1, du1, dv0);
        renderer->vertexUV(x0, y0, z1, du0, dv0);

        renderer->end();
    }

    for (const auto &kv : part->getChildren()) renderPart(kv.second.get(), textureWidth, textureHeight);

    GlStateManager::popMatrix();
}

std::unique_ptr<ModelPart> Model::buildPartTree(const ModelPartDefinition &definition) {
    std::unique_ptr<ModelPart> part = std::make_unique<ModelPart>(definition.getName());
    part->setPivot(definition.getPivot());
    part->setRotation(definition.getRotation());
    part->setPosition(definition.getPosition());

    for (const ModelPartCubeDefinition &cubeDefinition : definition.getCubes()) {
        Vec3 min      = cubeDefinition.getMin();
        Vec3 max      = cubeDefinition.getMax();
        float inflate = cubeDefinition.getInflate();

        ModelPart::Cube cube;
        cube.min = Vec3(min.x - inflate, min.y - inflate, min.z - inflate);
        cube.max = Vec3(max.x + inflate, max.y + inflate, max.z + inflate);
        cube.uv  = cubeDefinition.getUV();

        part->addCube(cube);
    }

    for (const auto &kv : definition.getChildren()) part->addChild(buildPartTree(*kv.second));

    return part;
}

std::unique_ptr<ModelPart> Model::clonePartTree(const ModelPart *part) {
    std::unique_ptr<ModelPart> copy = std::make_unique<ModelPart>(part->getName());

    copy->setPivot(part->getPivot());
    copy->setRotation(part->getRotation());
    copy->setPosition(part->getPosition());

    for (const ModelPart::Cube &cube : part->getCubes()) copy->addCube(cube);

    for (const auto &kv : part->getChildren()) copy->addChild(clonePartTree(kv.second.get()));

    return copy;
}
