#include "LivingEntity.h"

LivingEntity::LivingEntity(World *world) : Entity(world) {}

LivingEntity::~LivingEntity() {}

uint64_t LivingEntity::getType() { return TYPE; }
