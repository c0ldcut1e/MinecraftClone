#include "TestEntity.h"

#include <algorithm>
#include <cmath>

#include "../utils/math/Mth.h"
#include "../world/models/ModelRegistry.h"

TestEntity::TestEntity(Level *level)
    : LivingEntity(level), m_hasLastPos(false), m_lastPos(Vec3(0.0)), m_limbSwing(0.0f),
      m_limbSwingAmount(0.0f), m_limbSwingOld(0.0f), m_limbSwingAmountOld(0.0f)
{
    setModel(ModelRegistry::get()->getModel("steve")->copy());
}

TestEntity::~TestEntity() {}

uint64_t TestEntity::getType() { return TYPE; }

void TestEntity::tick()
{
    LivingEntity::tick();

    m_limbSwingOld       = m_limbSwing;
    m_limbSwingAmountOld = m_limbSwingAmount;

    float horizontalSpeed = sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
    float targetAmount    = horizontalSpeed / (float) m_maxSpeed * 8.0f;
    targetAmount          = Mth::clampf(targetAmount, 0.0f, 1.0f);
    targetAmount          = sqrtf(targetAmount);
    targetAmount =
            std::min(targetAmount,
                     0.5f + 0.35f * Mth::clampf(horizontalSpeed / (float) m_maxSpeed, 0.0f, 1.0f));

    m_limbSwingAmount += (targetAmount - m_limbSwingAmount) * 0.35f;
    float f = Mth::clampf(horizontalSpeed / (float) m_maxSpeed, 0.0f, 1.0f);
    m_limbSwing += 7.2f * sqrtf(f) * f + 0.8f * (1.0f - f) * sqrtf(f);
}

float TestEntity::getLimbSwing() const { return m_limbSwing; }

float TestEntity::getLimbSwingAmount() const { return m_limbSwingAmount; }

float TestEntity::getLimbSwingOld() const { return m_limbSwingOld; }

float TestEntity::getLimbSwingAmountOld() const { return m_limbSwingAmountOld; }
