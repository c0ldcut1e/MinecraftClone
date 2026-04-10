#pragma once

#include "../../rendering/Shader.h"
#include "UIComponent.h"

class UIComponent_Crosshair : public UIComponent
{
public:
    UIComponent_Crosshair();
    ~UIComponent_Crosshair() override;

    void tick() override;
    void render() override;

private:
    void ensureCaptureTexture(int captureWidth, int captureHeight);

    Shader m_shader;

    uint32_t m_vao;
    uint32_t m_vbo;

    uint32_t m_captureTexture;
    int m_captureWidth;
    int m_captureHeight;

    float m_alpha;
    float m_thickness;
    float m_gap;
    float m_arm;
    float m_size;
};
