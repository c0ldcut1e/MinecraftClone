#include "TestEntityRenderer.h"

#include <cmath>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../rendering/GlStateManager.h"
#include "../utils/math/Math.h"
#include "../world/models/ModelRegistry.h"
#include "TestEntity.h"

void TestEntityRenderer::draw(const Entity *entity, float alpha) const {
    const TestEntity *_entity = (const TestEntity *) entity;

    Vec3 pos = entity->getRenderPosition(alpha);

    constexpr float STEVE_MIN_Y_PX  = -2.0f;
    constexpr float STEVE_HEIGHT_PX = 32.0f;

    const AABB &box    = entity->getAABB();
    float entityHeight = box.getMax().y - box.getMin().y;

    float modelHeightBlocks = STEVE_HEIGHT_PX / 16.0f;
    float fitScale          = entityHeight / modelHeightBlocks;

    float finalScale = (1.0f / 16.0f) * fitScale;

    GlStateManager::pushMatrix();

    GlStateManager::translatef(pos.x, pos.y, pos.z);
    GlStateManager::scalef(finalScale, finalScale, finalScale);
    GlStateManager::translatef(0.0f, -STEVE_MIN_Y_PX, 0.0f);

    Model *model = _entity->getModel();
    if (model) {
        float limbSwing  = _entity->getLimbSwingOld() + (_entity->getLimbSwing() - _entity->getLimbSwingOld()) * alpha;
        float limbAmount = _entity->getLimbSwingAmountOld() + (_entity->getLimbSwingAmount() - _entity->getLimbSwingAmountOld()) * alpha;

        constexpr float ARM_SCALE = 2.0f;
        constexpr float LEG_SCALE = 1.4f;

        float phase = limbSwing * 0.6662f;

        float rightArmX = cosf(phase + (float) M_PI) * ARM_SCALE * limbAmount * 0.5f;
        float leftArmX  = cosf(phase) * ARM_SCALE * limbAmount * 0.5f;
        float rightLegX = cosf(phase) * LEG_SCALE * limbAmount;
        float leftLegX  = cosf(phase + (float) M_PI) * LEG_SCALE * limbAmount;

        ModelPart *rightArm = model->findPart("rightArm");
        ModelPart *leftArm  = model->findPart("leftArm");
        ModelPart *rightLeg = model->findPart("rightLeg");
        ModelPart *leftLeg  = model->findPart("leftLeg");

        if (rightArm) {
            Vec3 rot = rightArm->getRotation();
            rot.x    = Math::radiansToDegrees(rightArmX);
            rightArm->setRotation(rot);
        }

        if (leftArm) {
            Vec3 rot = leftArm->getRotation();
            rot.x    = Math::radiansToDegrees(leftArmX);
            leftArm->setRotation(rot);
        }

        if (rightLeg) {
            Vec3 rot = rightLeg->getRotation();
            rot.x    = Math::radiansToDegrees(rightLegX);
            rightLeg->setRotation(rot);
        }

        if (leftLeg) {
            Vec3 rot = leftLeg->getRotation();
            rot.x    = Math::radiansToDegrees(leftLegX);
            leftLeg->setRotation(rot);
        }

        model->draw(*ModelRegistry::get()->getTextures());
    }

    GlStateManager::popMatrix();

    if (m_drawBoundingBox) drawBoundingBox(entity, alpha);
}
