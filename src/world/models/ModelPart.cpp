#include "ModelPart.h"

ModelPart::ModelPart() : m_name(""), m_pivot(0.0, 0.0, 0.0), m_rotation(0.0, 0.0, 0.0), m_position(0.0, 0.0, 0.0) {}

ModelPart::ModelPart(const std::string &name) : m_name(name), m_pivot(0.0, 0.0, 0.0), m_rotation(0.0, 0.0, 0.0), m_position(0.0, 0.0, 0.0) {}

const std::string &ModelPart::getName() const { return m_name; }

void ModelPart::setPivot(const Vec3 &pivot) { m_pivot = pivot; }

void ModelPart::setRotation(const Vec3 &rotationDegrees) { m_rotation = rotationDegrees; }

void ModelPart::setPosition(const Vec3 &position) { m_position = position; }

const Vec3 &ModelPart::getPivot() const { return m_pivot; }

const Vec3 &ModelPart::getRotation() const { return m_rotation; }

const Vec3 &ModelPart::getPosition() const { return m_position; }

void ModelPart::addCube(const Cube &cube) { m_cubes.push_back(cube); }

const std::vector<ModelPart::Cube> &ModelPart::getCubes() const { return m_cubes; }

ModelPart *ModelPart::addChild(std::unique_ptr<ModelPart> child) {
    ModelPart *ptr               = child.get();
    m_children[child->getName()] = std::move(child);
    return ptr;
}

ModelPart *ModelPart::getChild(const std::string &name) {
    auto it = m_children.find(name);
    if (it == m_children.end()) return nullptr;
    return it->second.get();
}

const std::unordered_map<std::string, std::unique_ptr<ModelPart>> &ModelPart::getChildren() const { return m_children; }
