#include "EntityRenderer.h"

#include <glad/glad.h>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../rendering/Tesselator.h"
#include "Entity.h"

EntityRenderer::EntityRenderer() : m_renderBoundingBox(false) {}

uint32_t EntityRenderer::multiplyColor(uint32_t color, uint32_t lightColor)
{
    uint32_t a  = (color >> 24) & 0xFF;
    uint32_t r  = (color >> 16) & 0xFF;
    uint32_t g  = (color >> 8) & 0xFF;
    uint32_t b  = color & 0xFF;
    uint32_t lr = (lightColor >> 16) & 0xFF;
    uint32_t lg = (lightColor >> 8) & 0xFF;
    uint32_t lb = lightColor & 0xFF;

    r = (r * lr + 127) / 255;
    g = (g * lg + 127) / 255;
    b = (b * lb + 127) / 255;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

void EntityRenderer::render(const Entity *entity, float partialTicks, uint32_t lightColor) const
{
    AABB box        = entity->getBoundingBox().translated(entity->getRenderPosition(partialTicks));
    const Vec3 &min = box.getMin();
    const Vec3 &max = box.getMax();

    float y = min.y + 0.1f;

    float x  = min.x;
    float x2 = max.x;
    float z  = min.z;
    float z2 = max.z;

    BufferBuilder *renderer = Tesselator::getInstance()->getBuilderForLevel();
    renderer->begin(GL_TRIANGLES);
    renderer->color(multiplyColor(0xFF777777, lightColor));

    renderer->vertex(x, y, z);
    renderer->vertex(x2, y, z2);
    renderer->vertex(x2, y, z);

    renderer->vertex(x2, y, z2);
    renderer->vertex(x, y, z);
    renderer->vertex(x, y, z2);

    renderer->end();

    if (m_renderBoundingBox)
    {
        renderBoundingBox(entity, partialTicks);
    }
}

void EntityRenderer::setRenderBoundingBox(bool value) { m_renderBoundingBox = value; }

void EntityRenderer::renderBoundingBox(const Entity *entity, float partialTicks) const
{
    AABB box        = entity->getBoundingBox().translated(entity->getRenderPosition(partialTicks));
    const Vec3 &min = box.getMin();
    const Vec3 &max = box.getMax();

    BufferBuilder *renderer = Tesselator::getInstance()->getBuilderForLevel();
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
