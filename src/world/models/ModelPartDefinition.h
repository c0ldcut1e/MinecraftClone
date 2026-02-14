#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/math/Vec3.h"
#include "ModelPartCubeDefinition.h"

class ModelPartDefinition {
public:
    ModelPartDefinition();
    explicit ModelPartDefinition(const std::string &name);

    const std::string &getName() const;

    void setPivot(const Vec3 &pivot);
    void setRotation(const Vec3 &rotationDegrees);
    void setPosition(const Vec3 &position);

    const Vec3 &getPivot() const;
    const Vec3 &getRotation() const;
    const Vec3 &getPosition() const;

    void addCube(const ModelPartCubeDefinition &cube);
    const std::vector<ModelPartCubeDefinition> &getCubes() const;

    ModelPartDefinition *addChild(std::unique_ptr<ModelPartDefinition> child);
    ModelPartDefinition *getChild(const std::string &name);
    const std::unordered_map<std::string, std::unique_ptr<ModelPartDefinition>> &getChildren() const;

private:
    std::string m_name;
    Vec3 m_pivot;
    Vec3 m_rotation;
    Vec3 m_position;

    std::vector<ModelPartCubeDefinition> m_cubes;
    std::unordered_map<std::string, std::unique_ptr<ModelPartDefinition>> m_children;
};
