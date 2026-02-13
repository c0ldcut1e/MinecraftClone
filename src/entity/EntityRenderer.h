#pragma once

#include "Entity.h"

class EntityRenderer {
public:
    EntityRenderer()          = default;
    virtual ~EntityRenderer() = default;

    virtual void draw(const Entity *entity) const;
    void drawBoundingBox(const Entity *entity) const;
};
