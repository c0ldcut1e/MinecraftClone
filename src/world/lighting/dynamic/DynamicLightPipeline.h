#pragma once

#include <cstdint>
#include <vector>

#include "../../../utils/math/Mat4.h"
#include "../../chunk/ChunkPos.h"
#include "DynamicLight.h"

class Framebuffer;
class Chunk;
class Level;
class Shader;

class DynamicLightPipeline
{
public:
    explicit DynamicLightPipeline(int width, int height);
    ~DynamicLightPipeline();

    enum class PackedLightType : uint8_t
    {
        DIRECTIONAL,
        POINT,
        AREA
    };

    struct PackedLight
    {
        uint64_t id          = 0;
        PackedLightType type = PackedLightType::POINT;
        Vec3 color           = Vec3(1.0, 1.0, 1.0);
        Vec3 position;
        Vec3 direction   = Vec3(0.0, -1.0, 0.0);
        Vec3 right       = Vec3(1.0, 0.0, 0.0);
        Vec3 up          = Vec3(0.0, 1.0, 0.0);
        float brightness = 1.0f;
        float radius     = 15.0f;
        float width      = 1.0f;
        float height     = 1.0f;
        float angle      = 45.0f;
        float distance   = 15.0f;
    };

    void resize(int width, int height);

    Shader *beginAlbedoCapture(const Vec3 &cameraPos, const Mat4 &viewMatrix,
                               const Mat4 &projection, bool grassSideOverlayEnabled,
                               const Vec3 &chunkFadeColor);
    Shader *beginPositionCapture(const Mat4 &viewMatrix, const Mat4 &projection);
    Shader *beginNormalCapture(const Mat4 &viewMatrix, const Mat4 &projection);
    void prepareFrame(Level *level, const Vec3 &cameraPos);

    Framebuffer *apply(Level *level, Framebuffer *sceneFramebuffer);
    void renderImGui(Level *level);
    bool hasLightsToRender() const;

    void setEnabled(bool enabled);
    void toggleEnabled();
    bool isEnabled() const;

    size_t getVisibleLightCount() const;
    size_t getSubmittedLightCount() const;
    void copySubmittedLights(std::vector<PackedLight> *out) const;

private:
    enum class StageId
    {
        NONE,
        SCENE,
        ALBEDO,
        POSITION,
        NORMAL,
        LIGHT_COLOR,
        COMPOSITE
    };

    struct PipelineStage
    {
        StageId id         = StageId::NONE;
        const char *name   = "";
        uint32_t textureId = 0;
        int width          = 0;
        int height         = 0;
        bool active        = false;
    };

    static constexpr int MAX_LIGHTS = 64;

    void collectLights(Level *level, const Vec3 &cameraPos);
    DirectionalDynamicLight buildNewDirectionalLight() const;
    PointDynamicLight buildNewPointLight() const;
    AreaDynamicLight buildNewAreaLight() const;
    void packLights();
    int getLightBufferWidth(int width) const;
    int getLightBufferHeight(int height) const;
    void configureLightFramebufferFilters() const;
    void updateStage(PipelineStage *stage, Framebuffer *framebuffer, bool active);
    void bindLightUniforms(Shader *shader) const;
    void renderLightColor();
    void renderComposite(Framebuffer *sceneFramebuffer);
    void copyDepth(Framebuffer *sceneFramebuffer);
    void restoreRenderState() const;
    void renderDirectionalLightControls(Level *level);
    void renderPointLightControls(Level *level);
    void renderAreaLightControls(Level *level);
    void renderDirectionalLightEditor(Level *level, DirectionalDynamicLight light);
    void renderPointLightEditor(Level *level, PointDynamicLight light);
    void renderAreaLightEditor(Level *level, AreaDynamicLight light);

    Framebuffer *m_albedoFramebuffer;
    Framebuffer *m_positionFramebuffer;
    Framebuffer *m_normalFramebuffer;
    Framebuffer *m_lightColorFramebuffer;
    Framebuffer *m_outputFramebuffer;

    Shader *m_albedoShader;
    Shader *m_positionShader;
    Shader *m_normalShader;
    Shader *m_lightColorShader;
    Shader *m_compositeShader;

    bool m_enabled;
    float m_strength;
    float m_lightBufferScale;
    size_t m_lastVisibleLightCount;
    size_t m_lastSubmittedLightCount;
    PackedLightType m_selectedTabType;

    PipelineStage m_sceneStage;
    PipelineStage m_albedoStage;
    PipelineStage m_positionStage;
    PipelineStage m_normalStage;
    PipelineStage m_lightColorStage;
    PipelineStage m_outputStage;

    std::vector<PackedLight> m_visibleLights;
    std::vector<PackedLight> m_submittedLights;
    std::vector<float> m_lightPositionRadius;
    std::vector<float> m_lightColorBrightness;
    std::vector<float> m_lightDirectionType;
    std::vector<float> m_lightRightWidth;
    std::vector<float> m_lightUpHeight;
    std::vector<float> m_lightAreaAngleDistance;
};
