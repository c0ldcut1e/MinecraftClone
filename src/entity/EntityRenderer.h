#pragma once

#include "Entity.h"

class EntityRenderer {
public:
    EntityRenderer();
    virtual ~EntityRenderer() = default;

    virtual void render(const Entity *entity, float partialTicks) const;

    void setRenderBoundingBox(bool value);

protected:
    void renderBoundingBox(const Entity *entity, float partialTicks) const;

    bool m_renderBoundingBox;
};
