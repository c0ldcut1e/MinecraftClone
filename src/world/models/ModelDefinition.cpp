#include "ModelDefinition.h"

ModelDefinition::ModelDefinition() : m_id(""), m_skin(), m_root(std::make_unique<ModelPartDefinition>("root")) {}

ModelDefinition::ModelDefinition(const std::string &id, const ModelPartSkin &skin) : m_id(id), m_skin(skin), m_root(std::make_unique<ModelPartDefinition>("root")) {}

const std::string &ModelDefinition::getId() const { return m_id; }

void ModelDefinition::setSkin(const ModelPartSkin &skin) { m_skin = skin; }

const ModelPartSkin &ModelDefinition::getSkin() const { return m_skin; }

ModelPartDefinition *ModelDefinition::getRoot() { return m_root.get(); }

const ModelPartDefinition *ModelDefinition::getRoot() const { return m_root.get(); }
