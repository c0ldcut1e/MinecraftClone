#pragma once

#include "EntityRenderer.h"

class TestEntityRenderer : public EntityRenderer {
public:
    TestEntityRenderer()           = default;
    ~TestEntityRenderer() override = default;

    void render(const Entity *entity, float partialTicks) const override;
};
