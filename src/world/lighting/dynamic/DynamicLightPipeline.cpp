#include "DynamicLightPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>

#include "../../../core/Minecraft.h"
#include "../../../entity/LocalPlayer.h"
#include "../../../rendering/Framebuffer.h"
#include "../../../rendering/GlStateManager.h"
#include "../../../rendering/RenderCommand.h"
#include "../../../rendering/Shader.h"
#include "../../../ui/imgui/imgui.h"
#include "../../../world/Level.h"
#include "../../../world/block/BlockRegistry.h"

static constexpr float AMBIENT_LIGHT = 0.4f;

static const char *getLightTypeRegistryId(DynamicLightPipeline::PackedLightType type)
{
    switch (type)
    {
        case DynamicLightPipeline::PackedLightType::DIRECTIONAL:
            return "directional";
        case DynamicLightPipeline::PackedLightType::AREA:
            return "area";
        case DynamicLightPipeline::PackedLightType::POINT:
        default:
            return "point";
    }
}

static const char *getLightTypeName(DynamicLightPipeline::PackedLightType type)
{
    switch (type)
    {
        case DynamicLightPipeline::PackedLightType::DIRECTIONAL:
            return "Directional Light";
        case DynamicLightPipeline::PackedLightType::AREA:
            return "Area Light";
        case DynamicLightPipeline::PackedLightType::POINT:
        default:
            return "Point Light";
    }
}

static void appendVec4(std::vector<float> *data, float x, float y, float z, float w)
{
    data->push_back(x);
    data->push_back(y);
    data->push_back(z);
    data->push_back(w);
}

template<typename T>
static void appendLightEntry(std::vector<T> *lights, const T &light)
{
    lights->push_back(light);
}

template<typename T>
static void sortLightsById(std::vector<T> *lights)
{
    std::sort(lights->begin(), lights->end(), [](const T &a, const T &b) { return a.id < b.id; });
}

template<typename T>
static bool renderCommonLightControls(T *light, const char *typeId)
{
    bool changed = false;

    ImGui::Text("Handle %s#%llu", typeId, (unsigned long long) light->id);

    char nameBuffer[128];
    snprintf(nameBuffer, sizeof(nameBuffer), "%s", light->name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
    {
        light->name = nameBuffer;
        changed     = true;
    }

    if (ImGui::Checkbox("Enabled", &light->enabled))
    {
        changed = true;
    }

    float color[3] = {(float) light->color.x, (float) light->color.y, (float) light->color.z};
    if (ImGui::ColorEdit3("Color", color))
    {
        light->color = Vec3(color[0], color[1], color[2]);
        changed      = true;
    }

    if (ImGui::DragFloat("Brightness", &light->brightness, 0.02f))
    {
        changed = true;
    }

    return changed;
}

static Vec3 transformDirection(const Mat4 &matrix, const Vec3 &direction)
{
    return Vec3(matrix.data[0] * direction.x + matrix.data[4] * direction.y +
                        matrix.data[8] * direction.z,
                matrix.data[1] * direction.x + matrix.data[5] * direction.y +
                        matrix.data[9] * direction.z,
                matrix.data[2] * direction.x + matrix.data[6] * direction.y +
                        matrix.data[10] * direction.z);
}

static Mat4 buildOrientationMatrix(const Vec3 &orientation)
{
    double x = orientation.x * (M_PI / 180.0);
    double y = orientation.y * (M_PI / 180.0);
    double z = orientation.z * (M_PI / 180.0);

    Mat4 rotationX = Mat4::rotation(x, 1.0, 0.0, 0.0);
    Mat4 rotationY = Mat4::rotation(y, 0.0, 1.0, 0.0);
    Mat4 rotationZ = Mat4::rotation(z, 0.0, 0.0, 1.0);
    return rotationZ.multiply(rotationY).multiply(rotationX);
}

static double getLightReach(const DynamicLightPipeline::PackedLight &light)
{
    if (light.type == DynamicLightPipeline::PackedLightType::DIRECTIONAL)
    {
        return 1000000.0;
    }
    if (light.type == DynamicLightPipeline::PackedLightType::POINT)
    {
        return std::max(1.0, (double) light.radius);
    }

    double extent = std::max((double) light.width, (double) light.height);
    return std::max(1.0, sqrt((double) light.distance * (double) light.distance + extent * extent));
}

static void setFramebufferViewport(const Framebuffer *framebuffer)
{
    if (!framebuffer)
    {
        return;
    }

    RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
}

static Vec3 getCameraPositionOrFallback()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft || !minecraft->getCamera())
    {
        return Vec3(0.5, 137.0, 0.5);
    }

    return minecraft->getCamera()->getPosition();
}

static Vec3 getCameraLookOrFallback()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft || !minecraft->getLocalPlayer())
    {
        return Vec3(0.0, -1.0, 0.0);
    }

    return minecraft->getLocalPlayer()->getFront().normalize();
}

static Vec3 getCameraOrientationOrFallback()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return Vec3();
    }

    if (const Camera *camera = minecraft->getCamera())
    {
        Vec3 front   = camera->getFront().normalize();
        double pitch = asin(std::clamp(front.y, -1.0, 1.0)) * (180.0 / M_PI);
        double yaw   = -atan2(front.z, front.x) * (180.0 / M_PI) - 90.0;
        return Vec3(pitch, yaw, 0.0);
    }

    return Vec3();
}

DynamicLightPipeline::DynamicLightPipeline(int width, int height)
    : m_albedoFramebuffer(nullptr), m_positionFramebuffer(nullptr), m_normalFramebuffer(nullptr),
      m_lightColorFramebuffer(nullptr), m_outputFramebuffer(nullptr), m_albedoShader(nullptr),
      m_positionShader(nullptr), m_normalShader(nullptr), m_lightColorShader(nullptr),
      m_compositeShader(nullptr), m_enabled(true), m_strength(1.0f), m_lightBufferScale(1.0f),
      m_lastVisibleLightCount(0), m_lastSubmittedLightCount(0),
      m_selectedTabType(PackedLightType::AREA)
{
    m_albedoFramebuffer   = new Framebuffer(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_positionFramebuffer = new Framebuffer(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    m_normalFramebuffer   = new Framebuffer(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_lightColorFramebuffer =
            new Framebuffer(getLightBufferWidth(width), getLightBufferHeight(height), GL_RGBA16F,
                            GL_RGBA, GL_FLOAT);
    m_outputFramebuffer = new Framebuffer(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    configureLightFramebufferFilters();

    m_albedoShader =
            new Shader("shaders/dynamic_light_gbuffer.vert", "shaders/dynamic_light_albedo.frag");
    m_positionShader = new Shader("shaders/dynamic_light_gbuffer.vert",
                                  "shaders/dynamic_light_gbuffer_position.frag");
    m_normalShader   = new Shader("shaders/dynamic_light_gbuffer.vert",
                                  "shaders/dynamic_light_gbuffer_normal.frag");
    m_lightColorShader =
            new Shader("shaders/dynamic_light.vert", "shaders/dynamic_light_light_color.frag");
    m_compositeShader =
            new Shader("shaders/dynamic_light.vert", "shaders/dynamic_light_composite.frag");

    m_sceneStage.id        = StageId::SCENE;
    m_sceneStage.name      = "Scene";
    m_albedoStage.id       = StageId::ALBEDO;
    m_albedoStage.name     = "Albedo";
    m_positionStage.id     = StageId::POSITION;
    m_positionStage.name   = "Position";
    m_normalStage.id       = StageId::NORMAL;
    m_normalStage.name     = "Normal";
    m_lightColorStage.id   = StageId::LIGHT_COLOR;
    m_lightColorStage.name = "Light Color";
    m_outputStage.id       = StageId::COMPOSITE;
    m_outputStage.name     = "Composite";
}

DynamicLightPipeline::~DynamicLightPipeline()
{
    if (m_compositeShader)
    {
        delete m_compositeShader;
        m_compositeShader = nullptr;
    }
    if (m_lightColorShader)
    {
        delete m_lightColorShader;
        m_lightColorShader = nullptr;
    }
    if (m_positionShader)
    {
        delete m_positionShader;
        m_positionShader = nullptr;
    }
    if (m_normalShader)
    {
        delete m_normalShader;
        m_normalShader = nullptr;
    }
    if (m_albedoShader)
    {
        delete m_albedoShader;
        m_albedoShader = nullptr;
    }
    if (m_outputFramebuffer)
    {
        delete m_outputFramebuffer;
        m_outputFramebuffer = nullptr;
    }
    if (m_lightColorFramebuffer)
    {
        delete m_lightColorFramebuffer;
        m_lightColorFramebuffer = nullptr;
    }
    if (m_positionFramebuffer)
    {
        delete m_positionFramebuffer;
        m_positionFramebuffer = nullptr;
    }
    if (m_normalFramebuffer)
    {
        delete m_normalFramebuffer;
        m_normalFramebuffer = nullptr;
    }
    if (m_albedoFramebuffer)
    {
        delete m_albedoFramebuffer;
        m_albedoFramebuffer = nullptr;
    }
}

void DynamicLightPipeline::resize(int width, int height)
{
    if (m_albedoFramebuffer)
    {
        m_albedoFramebuffer->resize(width, height);
    }
    if (m_normalFramebuffer)
    {
        m_normalFramebuffer->resize(width, height);
    }
    if (m_positionFramebuffer)
    {
        m_positionFramebuffer->resize(width, height);
    }
    if (m_lightColorFramebuffer)
    {
        m_lightColorFramebuffer->resize(getLightBufferWidth(width), getLightBufferHeight(height));
    }
    if (m_outputFramebuffer)
    {
        m_outputFramebuffer->resize(width, height);
    }

    configureLightFramebufferFilters();
}

Shader *DynamicLightPipeline::beginAlbedoCapture(const Vec3 &cameraPos, const Mat4 &viewMatrix,
                                                 const Mat4 &projection,
                                                 bool grassSideOverlayEnabled,
                                                 const Vec3 &chunkFadeColor)
{
    updateStage(&m_albedoStage, m_albedoFramebuffer,
                hasLightsToRender() && m_albedoFramebuffer && m_albedoShader);

    if (!hasLightsToRender() || !m_albedoFramebuffer || !m_albedoShader)
    {
        return nullptr;
    }

    m_albedoFramebuffer->bind();
    setFramebufferViewport(m_albedoFramebuffer);
    RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderCommand::setClearDepth(1.0);
    RenderCommand::clearAll();

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    m_albedoShader->bind();
    m_albedoShader->setInt("u_texture", 0);
    m_albedoShader->setInt("u_grassOverlayTexture", 1);
    m_albedoShader->setInt("u_grassSideOverlayEnabled", grassSideOverlayEnabled ? 1 : 0);
    m_albedoShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_albedoShader->setMat4("u_view", viewMatrix.data);
    m_albedoShader->setMat4("u_projection", projection.data);
    m_albedoShader->setVec3("u_cameraPos", (float) cameraPos.x, (float) cameraPos.y,
                            (float) cameraPos.z);
    m_albedoShader->setVec3("u_chunkFadeColor", (float) chunkFadeColor.x, (float) chunkFadeColor.y,
                            (float) chunkFadeColor.z);
    m_albedoShader->setVec3("u_chunkOffset", 0.0f, 0.0f, 0.0f);
    m_albedoShader->setFloat("u_chunkAlpha", 1.0f);

    Texture *grassOverlayMask = BlockRegistry::getTextureRepository()
                                        ->get("textures/block/grass_side_overlay.png")
                                        .get();
    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(grassOverlayMask ? grassOverlayMask->getId() : 0);
    RenderCommand::activeTexture(0);

    return m_albedoShader;
}

Shader *DynamicLightPipeline::beginPositionCapture(const Mat4 &viewMatrix, const Mat4 &projection)
{
    updateStage(&m_positionStage, m_positionFramebuffer,
                hasLightsToRender() && m_positionFramebuffer && m_positionShader);

    if (!hasLightsToRender() || !m_positionFramebuffer || !m_positionShader)
    {
        return nullptr;
    }

    m_positionFramebuffer->bind();
    setFramebufferViewport(m_positionFramebuffer);
    RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderCommand::setClearDepth(1.0);
    RenderCommand::clearAll();

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    m_positionShader->bind();
    m_positionShader->setInt("u_texture", 0);
    m_positionShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_positionShader->setMat4("u_view", viewMatrix.data);
    m_positionShader->setMat4("u_projection", projection.data);
    m_positionShader->setVec3("u_chunkOffset", 0.0f, 0.0f, 0.0f);
    m_positionShader->setFloat("u_chunkAlpha", 1.0f);
    return m_positionShader;
}

Shader *DynamicLightPipeline::beginNormalCapture(const Mat4 &viewMatrix, const Mat4 &projection)
{
    updateStage(&m_normalStage, m_normalFramebuffer,
                hasLightsToRender() && m_normalFramebuffer && m_normalShader);

    if (!hasLightsToRender() || !m_normalFramebuffer || !m_normalShader)
    {
        return nullptr;
    }

    m_normalFramebuffer->bind();
    setFramebufferViewport(m_normalFramebuffer);
    RenderCommand::setClearColor(0.5f, 0.5f, 1.0f, 0.0f);
    RenderCommand::setClearDepth(1.0);
    RenderCommand::clearAll();

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    m_normalShader->bind();
    m_normalShader->setInt("u_texture", 0);
    m_normalShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_normalShader->setMat4("u_view", viewMatrix.data);
    m_normalShader->setMat4("u_projection", projection.data);
    m_normalShader->setVec3("u_chunkOffset", 0.0f, 0.0f, 0.0f);
    m_normalShader->setFloat("u_chunkAlpha", 1.0f);
    return m_normalShader;
}

void DynamicLightPipeline::prepareFrame(Level *level, const Vec3 &cameraPos)
{
    m_visibleLights.clear();
    m_submittedLights.clear();
    m_lastVisibleLightCount   = 0;
    m_lastSubmittedLightCount = 0;

    if (!m_enabled || !level)
    {
        return;
    }

    collectLights(level, cameraPos);
    packLights();
}

Framebuffer *DynamicLightPipeline::apply(Level *level, Framebuffer *sceneFramebuffer)
{
    updateStage(&m_sceneStage, sceneFramebuffer, sceneFramebuffer != nullptr);
    updateStage(&m_albedoStage, m_albedoFramebuffer,
                hasLightsToRender() && m_albedoFramebuffer && m_albedoShader);
    updateStage(&m_positionStage, m_positionFramebuffer,
                hasLightsToRender() && m_positionFramebuffer && m_positionShader);
    updateStage(&m_normalStage, m_normalFramebuffer,
                hasLightsToRender() && m_normalFramebuffer && m_normalShader);
    updateStage(&m_lightColorStage, m_lightColorFramebuffer, false);
    updateStage(&m_outputStage, m_outputFramebuffer, false);

    if (!sceneFramebuffer)
    {
        return nullptr;
    }

    if (!hasLightsToRender() || !level || !m_albedoFramebuffer || !m_positionFramebuffer ||
        !m_normalFramebuffer || !m_lightColorFramebuffer || !m_outputFramebuffer ||
        !m_albedoShader || !m_positionShader || !m_normalShader || !m_lightColorShader ||
        !m_compositeShader)
    {
        m_lastVisibleLightCount   = 0;
        m_lastSubmittedLightCount = 0;
        m_visibleLights.clear();
        m_submittedLights.clear();
        return sceneFramebuffer;
    }

    renderLightColor();
    renderComposite(sceneFramebuffer);
    copyDepth(sceneFramebuffer);
    restoreRenderState();

    updateStage(&m_lightColorStage, m_lightColorFramebuffer, true);
    updateStage(&m_outputStage, m_outputFramebuffer, true);
    setFramebufferViewport(m_outputFramebuffer);

    return m_outputFramebuffer;
}

void DynamicLightPipeline::renderImGui(Level *level)
{
    if (!ImGui::Begin("Dynamic Light"))
    {
        ImGui::End();
        return;
    }

    bool enabled = m_enabled;
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        m_enabled = enabled;
    }

    ImGui::SameLine();
    ImGui::Text("Visible %zu", m_lastVisibleLightCount);
    ImGui::SameLine();
    ImGui::Text("Submitted %zu", m_lastSubmittedLightCount);

    ImGui::SliderFloat("Strength", &m_strength, 0.0f, 4.0f);
    if (ImGui::SliderFloat("Resolution Scale", &m_lightBufferScale, 0.25f, 1.0f))
    {
        m_lightBufferScale = std::clamp(m_lightBufferScale, 0.25f, 1.0f);
        if (m_outputFramebuffer)
        {
            resize(m_outputFramebuffer->getWidth(), m_outputFramebuffer->getHeight());
        }
    }

    if (!level)
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add"))
    {
        if (m_selectedTabType == PackedLightType::AREA)
        {
            level->addAreaDynamicLight(buildNewAreaLight());
        }
        else if (m_selectedTabType == PackedLightType::DIRECTIONAL)
        {
            level->addDirectionalDynamicLight(buildNewDirectionalLight());
        }
        else
        {
            level->addPointDynamicLight(buildNewPointLight());
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove"))
    {
        if (m_selectedTabType == PackedLightType::AREA)
        {
            level->clearAreaDynamicLights();
        }
        else if (m_selectedTabType == PackedLightType::DIRECTIONAL)
        {
            level->clearDirectionalDynamicLights();
        }
        else
        {
            level->clearPointDynamicLights();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove All"))
    {
        level->clearDynamicLights();
    }

    if (ImGui::BeginTabBar("##dynamic_light_tabs"))
    {
        if (ImGui::BeginTabItem("Area"))
        {
            m_selectedTabType = PackedLightType::AREA;
            renderAreaLightControls(level);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Directional"))
        {
            m_selectedTabType = PackedLightType::DIRECTIONAL;
            renderDirectionalLightControls(level);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Point"))
        {
            m_selectedTabType = PackedLightType::POINT;
            renderPointLightControls(level);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

bool DynamicLightPipeline::hasLightsToRender() const
{
    return m_enabled && !m_submittedLights.empty();
}

void DynamicLightPipeline::setEnabled(bool enabled) { m_enabled = enabled; }

void DynamicLightPipeline::toggleEnabled() { m_enabled = !m_enabled; }

bool DynamicLightPipeline::isEnabled() const { return m_enabled; }

size_t DynamicLightPipeline::getVisibleLightCount() const { return m_lastVisibleLightCount; }

size_t DynamicLightPipeline::getSubmittedLightCount() const { return m_lastSubmittedLightCount; }

void DynamicLightPipeline::copySubmittedLights(std::vector<PackedLight> *out) const
{
    if (!out)
    {
        return;
    }

    *out = m_submittedLights;
}

void DynamicLightPipeline::collectLights(Level *level, const Vec3 &cameraPos)
{
    m_visibleLights.clear();
    m_submittedLights.clear();

    if (!level)
    {
        m_lastVisibleLightCount   = 0;
        m_lastSubmittedLightCount = 0;
        return;
    }

    std::vector<DirectionalDynamicLight> directionalLights;
    std::vector<PointDynamicLight> pointLights;
    std::vector<AreaDynamicLight> areaLights;
    level->copyDirectionalDynamicLights(&directionalLights);
    level->copyPointDynamicLights(&pointLights);
    level->copyAreaDynamicLights(&areaLights);

    for (const DirectionalDynamicLight &light : directionalLights)
    {
        if (!light.enabled)
        {
            continue;
        }
        if (light.renderInChunk && !level->hasChunk(light.chunkPos))
        {
            continue;
        }

        PackedLight packed;
        packed.id         = light.id;
        packed.type       = PackedLightType::DIRECTIONAL;
        packed.color      = light.color;
        packed.brightness = light.brightness;
        packed.direction  = light.direction.normalize();
        appendLightEntry(&m_visibleLights, packed);
    }

    for (const PointDynamicLight &light : pointLights)
    {
        if (!light.enabled)
        {
            continue;
        }
        if (light.renderInChunk && !level->hasChunk(light.chunkPos))
        {
            continue;
        }

        PackedLight packed;
        packed.id         = light.id;
        packed.type       = PackedLightType::POINT;
        packed.color      = light.color;
        packed.position   = light.position;
        packed.brightness = light.brightness;
        packed.radius     = std::max(0.1f, light.radius);
        appendLightEntry(&m_visibleLights, packed);
    }

    for (const AreaDynamicLight &light : areaLights)
    {
        if (!light.enabled)
        {
            continue;
        }
        if (light.renderInChunk && !level->hasChunk(light.chunkPos))
        {
            continue;
        }

        Mat4 orientationMatrix = buildOrientationMatrix(light.orientation);

        PackedLight packed;
        packed.id         = light.id;
        packed.type       = PackedLightType::AREA;
        packed.color      = light.color;
        packed.position   = light.position;
        packed.brightness = light.brightness;
        packed.direction  = transformDirection(orientationMatrix, Vec3(0.0, 0.0, -1.0)).normalize();
        packed.right      = transformDirection(orientationMatrix, Vec3(1.0, 0.0, 0.0)).normalize();
        packed.up         = transformDirection(orientationMatrix, Vec3(0.0, 1.0, 0.0)).normalize();
        packed.width      = std::max(0.01f, light.width);
        packed.height     = std::max(0.01f, light.height);
        packed.angle      = std::clamp(light.angle, 0.1f, 180.0f);
        packed.distance   = std::max(0.1f, light.distance);
        appendLightEntry(&m_visibleLights, packed);
    }

    std::sort(m_visibleLights.begin(), m_visibleLights.end(),
              [&cameraPos](const PackedLight &a, const PackedLight &b) {
                  double scoreA =
                          a.type == PackedLightType::DIRECTIONAL
                                  ? -1.0
                                  : a.position.sub(cameraPos).lengthSquared() /
                                            std::max(1.0, getLightReach(a) * getLightReach(a));
                  double scoreB =
                          b.type == PackedLightType::DIRECTIONAL
                                  ? -1.0
                                  : b.position.sub(cameraPos).lengthSquared() /
                                            std::max(1.0, getLightReach(b) * getLightReach(b));
                  if (scoreA != scoreB)
                  {
                      return scoreA < scoreB;
                  }

                  return a.brightness > b.brightness;
              });

    m_lastVisibleLightCount = m_visibleLights.size();

    size_t submittedCount = std::min((size_t) MAX_LIGHTS, m_visibleLights.size());
    m_submittedLights.assign(m_visibleLights.begin(), m_visibleLights.begin() + submittedCount);
    m_lastSubmittedLightCount = m_submittedLights.size();
}

DirectionalDynamicLight DynamicLightPipeline::buildNewDirectionalLight() const
{
    DirectionalDynamicLight light;
    light.name       = "Directional Light";
    light.color      = Vec3(1.0, 1.0, 1.0);
    light.brightness = 1.0f;
    light.direction  = Vec3(0.0, -1.0, 0.0);
    return light;
}

PointDynamicLight DynamicLightPipeline::buildNewPointLight() const
{
    PointDynamicLight light;
    light.name       = "Point Light";
    light.color      = Vec3(1.0, 0.78, 0.48);
    light.brightness = 1.0f;
    light.radius     = 15.0f;
    light.position   = getCameraPositionOrFallback();
    return light;
}

AreaDynamicLight DynamicLightPipeline::buildNewAreaLight() const
{
    AreaDynamicLight light;
    light.name        = "Area Light";
    light.color       = Vec3(1.0, 0.95, 0.85);
    light.brightness  = 1.0f;
    light.position    = getCameraPositionOrFallback();
    light.orientation = getCameraOrientationOrFallback();
    light.width       = 1.0f;
    light.height      = 1.0f;
    light.angle       = 45.0f;
    light.distance    = 15.0f;
    return light;
}

void DynamicLightPipeline::packLights()
{
    m_lightPositionRadius.clear();
    m_lightColorBrightness.clear();
    m_lightDirectionType.clear();
    m_lightRightWidth.clear();
    m_lightUpHeight.clear();
    m_lightAreaAngleDistance.clear();

    m_lightPositionRadius.reserve(m_submittedLights.size() * 4);
    m_lightColorBrightness.reserve(m_submittedLights.size() * 4);
    m_lightDirectionType.reserve(m_submittedLights.size() * 4);
    m_lightRightWidth.reserve(m_submittedLights.size() * 4);
    m_lightUpHeight.reserve(m_submittedLights.size() * 4);
    m_lightAreaAngleDistance.reserve(m_submittedLights.size() * 4);

    for (const PackedLight &light : m_submittedLights)
    {
        appendVec4(&m_lightPositionRadius, (float) light.position.x, (float) light.position.y,
                   (float) light.position.z, light.radius);
        appendVec4(&m_lightColorBrightness, (float) light.color.x, (float) light.color.y,
                   (float) light.color.z, light.brightness);
        appendVec4(&m_lightDirectionType, (float) light.direction.x, (float) light.direction.y,
                   (float) light.direction.z, (float) ((int) light.type));
        appendVec4(&m_lightRightWidth, (float) light.right.x, (float) light.right.y,
                   (float) light.right.z, light.width);
        appendVec4(&m_lightUpHeight, (float) light.up.x, (float) light.up.y, (float) light.up.z,
                   light.height);
        appendVec4(&m_lightAreaAngleDistance, cosf(light.angle * (float) (M_PI / 180.0f) * 0.5f),
                   light.distance, 0.0f, 0.0f);
    }
}

int DynamicLightPipeline::getLightBufferWidth(int width) const
{
    return std::max(1, (int) roundf((float) width * (float) m_lightBufferScale));
}

int DynamicLightPipeline::getLightBufferHeight(int height) const
{
    return std::max(1, (int) roundf((float) height * (float) m_lightBufferScale));
}

void DynamicLightPipeline::configureLightFramebufferFilters() const
{
    const Framebuffer *framebuffers[] = {m_lightColorFramebuffer};

    for (const Framebuffer *framebuffer : framebuffers)
    {
        if (!framebuffer)
        {
            continue;
        }

        RenderCommand::activeTexture(0);
        RenderCommand::bindTexture2D(framebuffer->getColorTexture());
        RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void DynamicLightPipeline::updateStage(PipelineStage *stage, Framebuffer *framebuffer, bool active)
{
    if (!stage)
    {
        return;
    }

    stage->textureId = framebuffer ? framebuffer->getColorTexture() : 0;
    stage->width     = framebuffer ? framebuffer->getWidth() : 0;
    stage->height    = framebuffer ? framebuffer->getHeight() : 0;
    stage->active    = active;
}

void DynamicLightPipeline::bindLightUniforms(Shader *shader) const
{
    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_positionFramebuffer->getColorTexture());
    shader->setInt("u_positionTexture", 0);

    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(m_normalFramebuffer->getColorTexture());
    shader->setInt("u_normalTexture", 1);

    shader->setInt("u_lightCount", (int) m_submittedLights.size());

    if (!m_submittedLights.empty())
    {
        shader->setVec4Array("u_lightPositionRadius", (int) m_submittedLights.size(),
                             m_lightPositionRadius.data());
        shader->setVec4Array("u_lightColorBrightness", (int) m_submittedLights.size(),
                             m_lightColorBrightness.data());
        shader->setVec4Array("u_lightDirectionType", (int) m_submittedLights.size(),
                             m_lightDirectionType.data());
        shader->setVec4Array("u_lightRightWidth", (int) m_submittedLights.size(),
                             m_lightRightWidth.data());
        shader->setVec4Array("u_lightUpHeight", (int) m_submittedLights.size(),
                             m_lightUpHeight.data());
        shader->setVec4Array("u_lightAreaAngleDistance", (int) m_submittedLights.size(),
                             m_lightAreaAngleDistance.data());
    }
}

void DynamicLightPipeline::renderLightColor()
{
    m_lightColorFramebuffer->bind();
    setFramebufferViewport(m_lightColorFramebuffer);
    RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderCommand::clearAll();
    GlStateManager::disableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();
    GlStateManager::disableBlend();

    m_lightColorShader->bind();
    bindLightUniforms(m_lightColorShader);
    m_lightColorShader->setFloat("u_ambientLight", AMBIENT_LIGHT);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);
    GlStateManager::setDepthMask(true);
}

void DynamicLightPipeline::renderComposite(Framebuffer *sceneFramebuffer)
{
    m_outputFramebuffer->bind();
    setFramebufferViewport(m_outputFramebuffer);
    RenderCommand::setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderCommand::clearAll();
    GlStateManager::disableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();
    GlStateManager::disableBlend();

    m_compositeShader->bind();

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(sceneFramebuffer->getColorTexture());
    m_compositeShader->setInt("u_sceneColorTexture", 0);

    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(m_albedoFramebuffer->getColorTexture());
    m_compositeShader->setInt("u_albedoTexture", 1);

    RenderCommand::activeTexture(2);
    RenderCommand::bindTexture2D(m_lightColorFramebuffer->getColorTexture());
    m_compositeShader->setInt("u_lightColorTexture", 2);

    m_compositeShader->setFloat("u_strength", m_strength);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);
    GlStateManager::setDepthMask(true);
}

void DynamicLightPipeline::copyDepth(Framebuffer *sceneFramebuffer)
{
    RenderCommand::bindFramebuffer(GL_READ_FRAMEBUFFER, sceneFramebuffer->getId());
    RenderCommand::bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outputFramebuffer->getId());
    RenderCommand::blitFramebuffer(
            0, 0, sceneFramebuffer->getWidth(), sceneFramebuffer->getHeight(), 0, 0,
            m_outputFramebuffer->getWidth(), m_outputFramebuffer->getHeight(), GL_DEPTH_BUFFER_BIT,
            GL_NEAREST);
    RenderCommand::bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    RenderCommand::bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void DynamicLightPipeline::restoreRenderState() const
{
    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::setColorMask(true, true, true, true);
}

void DynamicLightPipeline::renderDirectionalLightControls(Level *level)
{
    std::vector<DirectionalDynamicLight> lights;
    level->copyDirectionalDynamicLights(&lights);
    sortLightsById(&lights);

    for (const DirectionalDynamicLight &light : lights)
    {
        renderDirectionalLightEditor(level, light);
    }
}

void DynamicLightPipeline::renderPointLightControls(Level *level)
{
    std::vector<PointDynamicLight> lights;
    level->copyPointDynamicLights(&lights);
    sortLightsById(&lights);

    for (const PointDynamicLight &light : lights)
    {
        renderPointLightEditor(level, light);
    }
}

void DynamicLightPipeline::renderAreaLightControls(Level *level)
{
    std::vector<AreaDynamicLight> lights;
    level->copyAreaDynamicLights(&lights);
    sortLightsById(&lights);

    for (const AreaDynamicLight &light : lights)
    {
        renderAreaLightEditor(level, light);
    }
}

void DynamicLightPipeline::renderDirectionalLightEditor(Level *level, DirectionalDynamicLight light)
{
    bool open = true;
    char header[192];
    snprintf(header, sizeof(header), "%s [%s#%llu]###directional_%llu",
             light.name.empty() ? getLightTypeName(PackedLightType::DIRECTIONAL)
                                : light.name.c_str(),
             getLightTypeRegistryId(PackedLightType::DIRECTIONAL), (unsigned long long) light.id,
             (unsigned long long) light.id);

    ImGui::PushID((int) light.id);
    bool expanded = ImGui::CollapsingHeader(header, &open);
    if (!open)
    {
        level->removeDirectionalDynamicLight(light.id);
        ImGui::PopID();
        return;
    }

    if (expanded)
    {
        bool changed = renderCommonLightControls(
                &light, getLightTypeRegistryId(PackedLightType::DIRECTIONAL));

        float direction[3] = {(float) light.direction.x, (float) light.direction.y,
                              (float) light.direction.z};
        if (ImGui::SliderFloat3("Direction", direction, -1.0f, 1.0f))
        {
            light.direction = Vec3(direction[0], direction[1], direction[2]).normalize();
            changed         = true;
        }

        if (ImGui::Button("Set To Camera"))
        {
            light.direction = getCameraLookOrFallback();
            changed         = true;
        }

        if (changed)
        {
            light.brightness = std::max(0.0f, light.brightness);
            level->updateDirectionalDynamicLight(light);
        }
    }

    ImGui::PopID();
}

void DynamicLightPipeline::renderPointLightEditor(Level *level, PointDynamicLight light)
{
    bool open = true;
    char header[192];
    snprintf(header, sizeof(header), "%s [%s#%llu]###point_%llu",
             light.name.empty() ? getLightTypeName(PackedLightType::POINT) : light.name.c_str(),
             getLightTypeRegistryId(PackedLightType::POINT), (unsigned long long) light.id,
             (unsigned long long) light.id);

    ImGui::PushID((int) light.id);
    bool expanded = ImGui::CollapsingHeader(header, &open);
    if (!open)
    {
        level->removePointDynamicLight(light.id);
        ImGui::PopID();
        return;
    }

    if (expanded)
    {
        bool changed =
                renderCommonLightControls(&light, getLightTypeRegistryId(PackedLightType::POINT));

        float position[3] = {(float) light.position.x, (float) light.position.y,
                             (float) light.position.z};
        if (ImGui::DragFloat3("Position", position, 0.1f))
        {
            light.position = Vec3(position[0], position[1], position[2]);
            changed        = true;
        }

        if (ImGui::DragFloat("Radius", &light.radius, 0.05f))
        {
            changed = true;
        }

        if (ImGui::Button("Set To Camera"))
        {
            light.position = getCameraPositionOrFallback();
            changed        = true;
        }

        if (changed)
        {
            light.radius     = std::max(0.1f, light.radius);
            light.brightness = std::max(0.0f, light.brightness);
            level->updatePointDynamicLight(light);
        }
    }

    ImGui::PopID();
}

void DynamicLightPipeline::renderAreaLightEditor(Level *level, AreaDynamicLight light)
{
    bool open = true;
    char header[192];
    snprintf(header, sizeof(header), "%s [%s#%llu]###area_%llu",
             light.name.empty() ? getLightTypeName(PackedLightType::AREA) : light.name.c_str(),
             getLightTypeRegistryId(PackedLightType::AREA), (unsigned long long) light.id,
             (unsigned long long) light.id);

    ImGui::PushID((int) light.id);
    bool expanded = ImGui::CollapsingHeader(header, &open);
    if (!open)
    {
        level->removeAreaDynamicLight(light.id);
        ImGui::PopID();
        return;
    }

    if (expanded)
    {
        bool changed =
                renderCommonLightControls(&light, getLightTypeRegistryId(PackedLightType::AREA));

        float position[3] = {(float) light.position.x, (float) light.position.y,
                             (float) light.position.z};
        if (ImGui::DragFloat3("Position", position, 0.1f))
        {
            light.position = Vec3(position[0], position[1], position[2]);
            changed        = true;
        }

        float orientation[3] = {(float) light.orientation.x, (float) light.orientation.y,
                                (float) light.orientation.z};
        if (ImGui::DragFloat3("Orientation", orientation, 0.5f))
        {
            light.orientation = Vec3(orientation[0], orientation[1], orientation[2]);
            changed           = true;
        }

        if (ImGui::DragFloat("Width", &light.width, 0.02f))
        {
            changed = true;
        }
        if (ImGui::DragFloat("Height", &light.height, 0.02f))
        {
            changed = true;
        }
        if (ImGui::DragFloat("Angle", &light.angle, 0.1f))
        {
            changed = true;
        }
        if (ImGui::DragFloat("Distance", &light.distance, 0.05f))
        {
            changed = true;
        }

        if (ImGui::Button("Set To Camera"))
        {
            light.position    = getCameraPositionOrFallback();
            light.orientation = getCameraOrientationOrFallback();
            changed           = true;
        }

        if (changed)
        {
            light.width      = std::max(0.01f, light.width);
            light.height     = std::max(0.01f, light.height);
            light.angle      = std::clamp(light.angle, 0.1f, 179.9f);
            light.distance   = std::max(0.1f, light.distance);
            light.brightness = std::max(0.0f, light.brightness);
            level->updateAreaDynamicLight(light);
        }
    }

    ImGui::PopID();
}
