#include "LivingEntity.h"

LivingEntity::LivingEntity(Level *level) : Entity(level) {}

LivingEntity::~LivingEntity() {}

uint64_t LivingEntity::getType() { return TYPE; }
