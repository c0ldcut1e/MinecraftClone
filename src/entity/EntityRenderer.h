#pragma once

#include "Entity.h"

class EntityRenderer {
public:
    EntityRenderer();
    virtual ~EntityRenderer() = default;

    virtual void draw(const Entity *entity) const;

    void setDrawBoundingBox(bool value);

private:
    void drawBoundingBox(const Entity *entity) const;

    bool m_drawBoundingBox;
};
