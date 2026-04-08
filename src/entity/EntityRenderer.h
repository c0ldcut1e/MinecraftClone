#pragma once

#include "Entity.h"

class EntityRenderer
{
public:
    EntityRenderer();
    virtual ~EntityRenderer() = default;

    virtual void render(const Entity *entity, float partialTicks, uint32_t lightColor) const;

    void setRenderBoundingBox(bool value);

protected:
    void renderBoundingBox(const Entity *entity, float partialTicks) const;
    static uint32_t multiplyColor(uint32_t color, uint32_t lightColor);

    bool m_renderBoundingBox;
};
