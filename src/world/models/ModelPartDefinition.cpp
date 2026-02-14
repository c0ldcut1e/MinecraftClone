#include "ModelPartDefinition.h"

ModelPartDefinition::ModelPartDefinition() : m_name(""), m_pivot(0.0, 0.0, 0.0), m_rotation(0.0, 0.0, 0.0), m_position(0.0, 0.0, 0.0) {}

ModelPartDefinition::ModelPartDefinition(const std::string &name) : m_name(name), m_pivot(0.0, 0.0, 0.0), m_rotation(0.0, 0.0, 0.0), m_position(0.0, 0.0, 0.0) {}

const std::string &ModelPartDefinition::getName() const { return m_name; }

void ModelPartDefinition::setPivot(const Vec3 &pivot) { m_pivot = pivot; }

void ModelPartDefinition::setRotation(const Vec3 &rotationDegrees) { m_rotation = rotationDegrees; }

void ModelPartDefinition::setPosition(const Vec3 &position) { m_position = position; }

const Vec3 &ModelPartDefinition::getPivot() const { return m_pivot; }

const Vec3 &ModelPartDefinition::getRotation() const { return m_rotation; }

const Vec3 &ModelPartDefinition::getPosition() const { return m_position; }

void ModelPartDefinition::addCube(const ModelPartCubeDefinition &cube) { m_cubes.push_back(cube); }

const std::vector<ModelPartCubeDefinition> &ModelPartDefinition::getCubes() const { return m_cubes; }

ModelPartDefinition *ModelPartDefinition::addChild(std::unique_ptr<ModelPartDefinition> child) {
    ModelPartDefinition *ptr     = child.get();
    m_children[child->getName()] = std::move(child);
    return ptr;
}

ModelPartDefinition *ModelPartDefinition::getChild(const std::string &name) {
    auto it = m_children.find(name);
    if (it == m_children.end()) return nullptr;
    return it->second.get();
}

const std::unordered_map<std::string, std::unique_ptr<ModelPartDefinition>> &ModelPartDefinition::getChildren() const { return m_children; }
