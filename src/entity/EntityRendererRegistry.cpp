#include "EntityRendererRegistry.h"

#include "Entity.h"
#include "LivingEntity.h"
#include "LocalPlayer.h"
#include "Player.h"
#include "TestEntity.h"
#include "TestEntityRenderer.h"

EntityRendererRegistry *EntityRendererRegistry::get() {
    static EntityRendererRegistry instance;
    return &instance;
}

void EntityRendererRegistry::init() {
    EntityRendererRegistry *registry = get();
    registry->registerValue(Entity::TYPE, new EntityRenderer());
    registry->registerValue(LivingEntity::TYPE, new EntityRenderer());
    registry->registerValue(Player::TYPE, new EntityRenderer());
    registry->registerValue(LocalPlayer::TYPE, new EntityRenderer());
    registry->registerValue(TestEntity::TYPE, new TestEntityRenderer());
}
