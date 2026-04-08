#include "LevelRenderer.h"

#include <cmath>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/Tesselator.h"
#include "block/BlockRegistry.h"
#include "particle/ParticleRegistry.h"

static void emitRenderQuad(BufferBuilder *builder, const Vec3 &p1, const Vec3 &p2, const Vec3 &p3,
                           const Vec3 &p4, float u0, float v0, float u1, float v1, uint32_t color)
{
    builder->color(color);
    builder->vertexUV((float) p1.x, (float) p1.y, (float) p1.z, u0, v1);
    builder->vertexUV((float) p2.x, (float) p2.y, (float) p2.z, u0, v0);
    builder->vertexUV((float) p3.x, (float) p3.y, (float) p3.z, u1, v0);
    builder->vertexUV((float) p1.x, (float) p1.y, (float) p1.z, u0, v1);
    builder->vertexUV((float) p3.x, (float) p3.y, (float) p3.z, u1, v0);
    builder->vertexUV((float) p4.x, (float) p4.y, (float) p4.z, u1, v1);
}

static void emitRenderLine(BufferBuilder *builder, const Vec3 &p1, const Vec3 &p2, uint32_t color)
{
    builder->color(color);
    builder->vertex((float) p1.x, (float) p1.y, (float) p1.z);
    builder->vertex((float) p2.x, (float) p2.y, (float) p2.z);
}

static void emitRenderBoxOutline(BufferBuilder *builder, const Vec3 &min, const Vec3 &max,
                                 uint32_t color)
{
    Vec3 v000(min.x, min.y, min.z);
    Vec3 v001(min.x, min.y, max.z);
    Vec3 v010(min.x, max.y, min.z);
    Vec3 v011(min.x, max.y, max.z);
    Vec3 v100(max.x, min.y, min.z);
    Vec3 v101(max.x, min.y, max.z);
    Vec3 v110(max.x, max.y, min.z);
    Vec3 v111(max.x, max.y, max.z);

    emitRenderLine(builder, v000, v001, color);
    emitRenderLine(builder, v000, v010, color);
    emitRenderLine(builder, v000, v100, color);
    emitRenderLine(builder, v001, v011, color);
    emitRenderLine(builder, v001, v101, color);
    emitRenderLine(builder, v010, v011, color);
    emitRenderLine(builder, v010, v110, color);
    emitRenderLine(builder, v100, v101, color);
    emitRenderLine(builder, v100, v110, color);
    emitRenderLine(builder, v011, v111, color);
    emitRenderLine(builder, v101, v111, color);
    emitRenderLine(builder, v110, v111, color);
}

static void emitRenderBoxFilled(BufferBuilder *builder, const Vec3 &min, const Vec3 &max,
                                uint32_t color)
{
    emitRenderQuad(builder, Vec3(min.x, min.y, max.z), Vec3(min.x, max.y, max.z),
                   Vec3(max.x, max.y, max.z), Vec3(max.x, min.y, max.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
    emitRenderQuad(builder, Vec3(max.x, min.y, min.z), Vec3(max.x, max.y, min.z),
                   Vec3(min.x, max.y, min.z), Vec3(min.x, min.y, min.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
    emitRenderQuad(builder, Vec3(min.x, min.y, min.z), Vec3(min.x, max.y, min.z),
                   Vec3(min.x, max.y, max.z), Vec3(min.x, min.y, max.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
    emitRenderQuad(builder, Vec3(max.x, min.y, max.z), Vec3(max.x, max.y, max.z),
                   Vec3(max.x, max.y, min.z), Vec3(max.x, min.y, min.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
    emitRenderQuad(builder, Vec3(min.x, max.y, max.z), Vec3(min.x, max.y, min.z),
                   Vec3(max.x, max.y, min.z), Vec3(max.x, max.y, max.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
    emitRenderQuad(builder, Vec3(min.x, min.y, min.z), Vec3(min.x, min.y, max.z),
                   Vec3(max.x, min.y, max.z), Vec3(max.x, min.y, min.z), 0.0f, 0.0f, 1.0f, 1.0f,
                   color);
}

void LevelRenderer::renderRenderObjects(LevelRenderObjectStage stage)
{
    std::vector<LevelRenderObject> objects;
    m_level->copyRenderObjects(&objects);
    if (objects.empty())
    {
        return;
    }

    std::stable_sort(objects.begin(), objects.end(),
                     [](const LevelRenderObject &a, const LevelRenderObject &b) {
                         if (a.renderOrder != b.renderOrder)
                         {
                             return a.renderOrder < b.renderOrder;
                         }

                         return a.id < b.id;
                     });

    Minecraft *minecraft   = Minecraft::getInstance();
    Vec3 front             = minecraft->getLocalPlayer()->getFront().normalize();
    Vec3 worldUp           = fabs(front.y) > 0.99 ? Vec3(0.0, 0.0, 1.0) : Vec3(0.0, 1.0, 0.0);
    Vec3 right             = front.cross(worldUp).normalize();
    Vec3 up                = right.cross(front).normalize();
    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForLevel();

    for (const LevelRenderObject &object : objects)
    {
        if (object.stage != stage)
        {
            continue;
        }

        if (object.renderInChunk && !m_level->hasChunk(object.chunkPos))
        {
            continue;
        }

        std::shared_ptr<Texture> textureRef;
        Texture *texture = nullptr;

        if (object.textureSource == LevelRenderTextureSource::BLOCK_ATLAS)
        {
            TextureAtlas *atlas = BlockRegistry::getTextureAtlas();
            texture             = atlas ? atlas->getTexture() : nullptr;
        }
        else if (object.textureSource == LevelRenderTextureSource::PARTICLE_ATLAS)
        {
            texture = ParticleRegistry::getTexture();
        }
        else if (object.textureSource == LevelRenderTextureSource::PATH)
        {
            textureRef = BlockRegistry::getTextureRepository()->get(object.texturePath);
            texture    = textureRef.get();
        }

        if (object.textureSource != LevelRenderTextureSource::NONE && !texture)
        {
            continue;
        }

        if (object.depthTest)
        {
            GlStateManager::enableDepthTest();
        }
        else
        {
            GlStateManager::disableDepthTest();
        }

        if (object.blend)
        {
            GlStateManager::enableBlend();
            GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            GlStateManager::disableBlend();
        }

        GlStateManager::setDepthMask(object.depthWrite);

        if (object.cull)
        {
            GlStateManager::enableCull();
        }
        else
        {
            GlStateManager::disableCull();
        }

        if (object.type == LevelRenderObjectType::LINE ||
            (object.type == LevelRenderObjectType::BOX && !object.filled))
        {
            GlStateManager::setLineWidth(object.lineWidth);
        }
        else
        {
            GlStateManager::setLineWidth(1.0f);
        }

        if (texture)
        {
            builder->bindTexture(texture);
        }
        else
        {
            builder->unbindTexture();
        }
        builder->setAlphaCutoff(object.alphaCutoff);

        if (object.type == LevelRenderObjectType::LINE)
        {
            builder->begin(GL_LINES);
            emitRenderLine(builder, object.p1, object.p2, object.color);
            builder->end();
        }
        else if (object.type == LevelRenderObjectType::QUAD)
        {
            builder->begin(GL_TRIANGLES);
            emitRenderQuad(builder, object.p1, object.p2, object.p3, object.p4, object.u0,
                           object.v0, object.u1, object.v1, object.color);
            builder->end();
        }
        else if (object.type == LevelRenderObjectType::BILLBOARD)
        {
            Vec3 center = object.p1;
            Vec3 dx     = right.scale(object.p2.x * 0.5);
            Vec3 dy     = up.scale(object.p2.y * 0.5);
            Vec3 p1     = center.sub(dx).sub(dy);
            Vec3 p2     = center.sub(dx).add(dy);
            Vec3 p3     = center.add(dx).add(dy);
            Vec3 p4     = center.add(dx).sub(dy);

            builder->begin(GL_TRIANGLES);
            emitRenderQuad(builder, p1, p2, p3, p4, object.u0, object.v0, object.u1, object.v1,
                           object.color);
            builder->end();
        }
        else if (object.type == LevelRenderObjectType::BOX)
        {
            if (object.filled)
            {
                builder->begin(GL_TRIANGLES);
                emitRenderBoxFilled(builder, object.p1, object.p2, object.color);
                builder->end();
            }
            else
            {
                builder->begin(GL_LINES);
                emitRenderBoxOutline(builder, object.p1, object.p2, object.color);
                builder->end();
            }
        }

        builder->unbindTexture();
        builder->setAlphaCutoff(-1.0f);
    }

    GlStateManager::setLineWidth(1.0f);
    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
}
