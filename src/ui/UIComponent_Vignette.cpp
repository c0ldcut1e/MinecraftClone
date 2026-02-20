#include "UIComponent_Vignette.h"

#include <cmath>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"
#include "../utils/math/Math.h"

UIComponent_Vignette::UIComponent_Vignette() : UIComponent("ComponentVignette"), m_shader("shaders/vignette.vert", "shaders/vignette.frag"), m_texture("textures/misc/vignette.png"), m_vao(0), m_vbo(0) {
    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    float verts[6][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};

    RenderCommand::uploadArrayBuffer(verts, sizeof(verts), GL_STATIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), 0);

    m_shader.bind();
    m_shader.setInt("u_vignette", 0);
    m_shader.setFloat("u_strength", 1.0f);
}

UIComponent_Vignette::~UIComponent_Vignette() {
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void UIComponent_Vignette::render() {
    Minecraft *mc       = Minecraft::getInstance();
    LocalPlayer *player = mc ? mc->getLocalPlayer() : nullptr;
    World *world        = player ? player->getWorld() : nullptr;

    float strength = 0.0f;

    Vec3 playerPos = player->getPosition();
    BlockPos pos((int) floor(playerPos.x), (int) floor(playerPos.y), (int) floor(playerPos.z));

    uint8_t light = world->getLightLevel(pos);
    float light01 = std::min((float) light / 15.0f, 1.0f);

    float brightness = light01 * light01;

    strength = 1.0f - brightness;
    strength = Math::clampf(strength, 0.0f, 1.0f);
    strength = powf(strength, 1.4f);

    GlStateManager::disableDepthTest();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_ZERO, GL_SRC_COLOR);

    m_shader.bind();
    m_shader.setFloat("u_strength", strength);

    m_texture.bind(0);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, 6);

    GlStateManager::disableBlend();
    GlStateManager::enableDepthTest();
}
