#pragma once

#include "../rendering/Shader.h"
#include "../rendering/Texture.h"
#include "UIComponent.h"

class UIComponent_Vignette : public UIComponent {
public:
    UIComponent_Vignette();
    ~UIComponent_Vignette() override;

    void render() override;

private:
    Shader m_shader;
    Texture m_texture;
    uint32_t m_vao;
    uint32_t m_vbo;
};
