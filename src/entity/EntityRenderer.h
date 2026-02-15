#pragma once

#include "Entity.h"

class EntityRenderer {
public:
    EntityRenderer();
    virtual ~EntityRenderer() = default;

    virtual void draw(const Entity *entity, float alpha) const;

    void setDrawBoundingBox(bool value);

protected:
    void drawBoundingBox(const Entity *entity, float alpha) const;

    bool m_drawBoundingBox;
};
