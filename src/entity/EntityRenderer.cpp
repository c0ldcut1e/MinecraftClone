#include "EntityRenderer.h"

#include <glad/glad.h>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../rendering/ImmediateRenderer.h"
#include "Entity.h"

EntityRenderer::EntityRenderer() : m_renderBoundingBox(false) {}

void EntityRenderer::render(const Entity *entity, float partialTicks) const {
    AABB box        = entity->getBoundingBox().translated(entity->getRenderPosition(partialTicks));
    const Vec3 &min = box.getMin();
    const Vec3 &max = box.getMax();

    float y = min.y + 0.1f;

    float x  = min.x;
    float x2 = max.x;
    float z  = min.z;
    float z2 = max.z;

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->begin(GL_TRIANGLES);
    renderer->color(0xFF777777);

    renderer->vertex(x, y, z);
    renderer->vertex(x2, y, z);
    renderer->vertex(x2, y, z2);

    renderer->vertex(x2, y, z2);
    renderer->vertex(x, y, z2);
    renderer->vertex(x, y, z);

    renderer->end();

    if (m_renderBoundingBox) renderBoundingBox(entity, partialTicks);
}

void EntityRenderer::setRenderBoundingBox(bool value) { m_renderBoundingBox = value; }

void EntityRenderer::renderBoundingBox(const Entity *entity, float partialTicks) const {
    AABB box        = entity->getBoundingBox().translated(entity->getRenderPosition(partialTicks));
    const Vec3 &min = box.getMin();
    const Vec3 &max = box.getMax();

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->begin(GL_LINES);
    renderer->color(0xFFFFFFFF);

    renderer->vertex(min.x, min.y, min.z);
    renderer->vertex(max.x, min.y, min.z);

    renderer->vertex(max.x, min.y, min.z);
    renderer->vertex(max.x, min.y, max.z);

    renderer->vertex(max.x, min.y, max.z);
    renderer->vertex(min.x, min.y, max.z);

    renderer->vertex(min.x, min.y, max.z);
    renderer->vertex(min.x, min.y, min.z);

    renderer->vertex(min.x, max.y, min.z);
    renderer->vertex(max.x, max.y, min.z);

    renderer->vertex(max.x, max.y, min.z);
    renderer->vertex(max.x, max.y, max.z);

    renderer->vertex(max.x, max.y, max.z);
    renderer->vertex(min.x, max.y, max.z);

    renderer->vertex(min.x, max.y, max.z);
    renderer->vertex(min.x, max.y, min.z);

    renderer->vertex(min.x, min.y, min.z);
    renderer->vertex(min.x, max.y, min.z);

    renderer->vertex(max.x, min.y, min.z);
    renderer->vertex(max.x, max.y, min.z);

    renderer->vertex(max.x, min.y, max.z);
    renderer->vertex(max.x, max.y, max.z);

    renderer->vertex(min.x, min.y, max.z);
    renderer->vertex(min.x, max.y, max.z);

    renderer->end();
}
