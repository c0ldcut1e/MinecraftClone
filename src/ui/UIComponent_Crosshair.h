#pragma once

#include "../rendering/Shader.h"
#include "../ui/UIComponent.h"

class UIComponent_Crosshair : public UIComponent {
public:
    UIComponent_Crosshair();
    ~UIComponent_Crosshair() override;

    void tick() override;
    void render() override;

private:
    void ensureCaptureTexture(int captureSize);

    Shader m_shader;

    uint32_t m_vao;
    uint32_t m_vbo;

    uint32_t m_captureTexture;
    int m_captureSize;

    float m_alpha;
    float m_thickness;
    float m_gap;
    float m_arm;
    float m_size;
};
