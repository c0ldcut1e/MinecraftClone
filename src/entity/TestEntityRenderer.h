#pragma once

#include "EntityRenderer.h"

class TestEntityRenderer : public EntityRenderer {
public:
    TestEntityRenderer()           = default;
    ~TestEntityRenderer() override = default;

    void draw(const Entity *entity, float alpha) const override;
};
