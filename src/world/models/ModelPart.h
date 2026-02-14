#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/math/Vec3.h"
#include "ModelPartCubeDefinition.h"

class ModelPart {
public:
    struct Cube {
        Vec3 min;
        Vec3 max;
        ModelPartCubeDefinition::FaceUV uv;
    };

    ModelPart();
    explicit ModelPart(const std::string &name);

    const std::string &getName() const;

    void setPivot(const Vec3 &pivot);
    void setRotation(const Vec3 &rotationDegrees);
    void setPosition(const Vec3 &position);

    const Vec3 &getPivot() const;
    const Vec3 &getRotation() const;
    const Vec3 &getPosition() const;

    void addCube(const Cube &cube);
    const std::vector<Cube> &getCubes() const;

    ModelPart *addChild(std::unique_ptr<ModelPart> child);
    ModelPart *getChild(const std::string &name);
    const std::unordered_map<std::string, std::unique_ptr<ModelPart>> &getChildren() const;

private:
    std::string m_name;
    Vec3 m_pivot;
    Vec3 m_rotation;
    Vec3 m_position;

    std::vector<Cube> m_cubes;
    std::unordered_map<std::string, std::unique_ptr<ModelPart>> m_children;
};
