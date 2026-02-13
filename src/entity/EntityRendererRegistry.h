#pragma once

#include "../utils/TypedRegistry.h"
#include "EntityRenderer.h"

class EntityRendererRegistry : public TypedRegistry<uint64_t, EntityRenderer *> {
public:
    static EntityRendererRegistry *get();
    static void init();
};
