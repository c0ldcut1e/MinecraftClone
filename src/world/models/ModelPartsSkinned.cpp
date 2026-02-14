#include "ModelPartsSkinned.h"

static ModelPartCubeDefinition::FaceUV uvCube(int leftX, int leftY, int rightX, int rightY, int frontX, int frontY, int backX, int backY, int topX, int topY, int bottomX, int bottomY, int sideWidth, int sideHeight, int frontWidth, int frontHeight,
                                              int topWidth, int topHeight) {
    ModelPartCubeDefinition::FaceUV uv;
    uv.left   = {leftX, leftY, sideWidth, sideHeight};
    uv.right  = {rightX, rightY, sideWidth, sideHeight};
    uv.front  = {frontX, frontY, frontWidth, frontHeight};
    uv.back   = {backX, backY, frontWidth, frontHeight};
    uv.top    = {topX, topY, topWidth, topHeight};
    uv.bottom = {bottomX, bottomY, topWidth, topHeight};
    return uv;
}

ModelDefinition ModelPartsSkinned::createSteveClassic(const std::string &id, const std::string &texturePath) {
    ModelDefinition definition(id, ModelPartSkin(texturePath, 64, 64));
    ModelPartDefinition *root = definition.getRoot();

    std::unique_ptr<ModelPartDefinition> head = std::make_unique<ModelPartDefinition>("head");
    head->setPosition(Vec3(0.0, 30.0, 0.0));
    head->addCube(ModelPartCubeDefinition("head_cube", Vec3(-4.0, -8.0, -4.0), Vec3(4.0, 0.0, 4.0), uvCube(0, 8, 16, 8, 8, 8, 24, 8, 8, 0, 16, 0, 8, 8, 8, 8, 8, 8), 0.0f));

    std::unique_ptr<ModelPartDefinition> body = std::make_unique<ModelPartDefinition>("body");
    body->setPosition(Vec3(0.0, 30.0, 0.0));
    body->addCube(ModelPartCubeDefinition("body_cube", Vec3(-4.0, -20.0, -2.0), Vec3(4.0, -8.0, 2.0), uvCube(16, 20, 28, 20, 20, 20, 32, 20, 20, 16, 28, 16, 4, 12, 8, 12, 8, 4), 0.0f));

    std::unique_ptr<ModelPartDefinition> rightArm = std::make_unique<ModelPartDefinition>("rightArm");
    rightArm->setPosition(Vec3(-4.0, 22.0, 0.0));
    rightArm->addCube(ModelPartCubeDefinition("right_arm_cube", Vec3(-4.0, -12.0, -2.0), Vec3(0.0, 0.0, 2.0), uvCube(40, 20, 48, 20, 44, 20, 52, 20, 44, 16, 48, 16, 4, 12, 4, 12, 4, 4), 0.0f));

    std::unique_ptr<ModelPartDefinition> leftArm = std::make_unique<ModelPartDefinition>("leftArm");
    leftArm->setPosition(Vec3(4.0, 22.0, 0.0));
    leftArm->addCube(ModelPartCubeDefinition("left_arm_cube", Vec3(0.0, -12.0, -2.0), Vec3(4.0, 0.0, 2.0), uvCube(32, 52, 40, 52, 36, 52, 44, 52, 36, 48, 40, 48, 4, 12, 4, 12, 4, 4), 0.0f));

    std::unique_ptr<ModelPartDefinition> rightLeg = std::make_unique<ModelPartDefinition>("rightLeg");
    rightLeg->setPosition(Vec3(-2.0, 10.0, 0.0));
    rightLeg->addCube(ModelPartCubeDefinition("right_leg_cube", Vec3(-2.0, -12.0, -2.0), Vec3(2.0, 0.0, 2.0), uvCube(0, 20, 8, 20, 4, 20, 12, 20, 4, 16, 8, 16, 4, 12, 4, 12, 4, 4), 0.0f));

    std::unique_ptr<ModelPartDefinition> leftLeg = std::make_unique<ModelPartDefinition>("leftLeg");
    leftLeg->setPosition(Vec3(2.0, 10.0, 0.0));
    leftLeg->addCube(ModelPartCubeDefinition("left_leg_cube", Vec3(-2.0, -12.0, -2.0), Vec3(2.0, 0.0, 2.0), uvCube(16, 52, 24, 52, 20, 52, 28, 52, 20, 48, 24, 48, 4, 12, 4, 12, 4, 4), 0.0f));

    root->addChild(std::move(head));
    root->addChild(std::move(body));
    root->addChild(std::move(rightArm));
    root->addChild(std::move(leftArm));
    root->addChild(std::move(rightLeg));
    root->addChild(std::move(leftLeg));

    return definition;
}
