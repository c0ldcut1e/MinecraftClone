#include "ModelPartCubeDefinition.h"

ModelPartCubeDefinition::ModelPartCubeDefinition() : m_name(""), m_min(0.0, 0.0, 0.0), m_max(0.0, 0.0, 0.0), m_uv{}, m_inflate(0.0f) {}

ModelPartCubeDefinition::ModelPartCubeDefinition(const std::string &name, const Vec3 &min, const Vec3 &max, const FaceUV &uv, float inflate) : m_name(name), m_min(min), m_max(max), m_uv(uv), m_inflate(inflate) {}

const std::string &ModelPartCubeDefinition::getName() const { return m_name; }

const Vec3 &ModelPartCubeDefinition::getMin() const { return m_min; }

const Vec3 &ModelPartCubeDefinition::getMax() const { return m_max; }

const ModelPartCubeDefinition::FaceUV &ModelPartCubeDefinition::getUV() const { return m_uv; }

float ModelPartCubeDefinition::getInflate() const { return m_inflate; }
