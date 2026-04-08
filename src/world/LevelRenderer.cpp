#include "LevelRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>

#include "../core/Logger.h"
#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../memory/MemoryTracker.h"
#include "../rendering/ColormapManager.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/RenderCommand.h"
#include "../rendering/Tesselator.h"
#include "../threading/ThreadStorage.h"
#include "../utils/Random.h"
#include "../utils/Time.h"
#include "../utils/math/Mth.h"
#include "block/Block.h"
#include "block/BlockRegistry.h"
#include "lighting/LightEngine.h"
#include "lighting/LightSource.h"
#include "lighting/Lighting.h"
#include "lighting/dynamic/DynamicLightPipeline.h"
#include "models/Model.h"
#include "models/ModelDefinition.h"
#include "models/ModelPartsSkinned.h"
#include "particle/ParticleEngine.h"
#include "particle/ParticleRegistry.h"
#include "render/LevelRenderObject.h"

static Direction *decodeDirection(uint8_t face)
{
    if (face == 1)
    {
        return Direction::NORTH;
    }
    if (face == 2)
    {
        return Direction::SOUTH;
    }
    if (face == 3)
    {
        return Direction::WEST;
    }
    if (face == 4)
    {
        return Direction::EAST;
    }
    if (face == 5)
    {
        return Direction::DOWN;
    }
    if (face == 6)
    {
        return Direction::UP;
    }
    return nullptr;
}

static inline uint8_t decodeTintMode(uint32_t packedTint)
{
    return (uint8_t) ((packedTint >> 24) & 0xFF);
}

static uint32_t multiplyColorRgb(uint32_t color, uint32_t lightColor)
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

static double clamp01(double value) { return Mth::clamp(value, 0.0, 1.0); }

static double smoothstep(double edge0, double edge1, double x)
{
    if (fabs(edge1 - edge0) <= 0.000001)
    {
        return x < edge0 ? 0.0 : 1.0;
    }

    double t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

static double attenuateNoCusp(double distanceValue, double radiusValue)
{
    double safeRadius = std::max(radiusValue, 0.0001);
    double normalized = distanceValue / safeRadius;
    if (normalized >= 1.0)
    {
        return 0.0;
    }

    double oneMinusS = 1.0 - normalized;
    return oneMinusS * oneMinusS * oneMinusS;
}

static double computeConeScale(double coneCos)
{
    double safeCos = Mth::clamp(coneCos, 0.0001, 0.999999);
    return sqrt(std::max(1.0 - safeCos * safeCos, 0.0)) / safeCos;
}

static double computeAreaSpotMask(const DynamicLightPipeline::PackedLight &light,
                                  const Vec3 &toSurface)
{
    double forwardDistance = toSurface.dot(light.direction);
    if (forwardDistance <= 0.0001)
    {
        return 0.0;
    }

    double outerConeCos = cos(Mth::clamp((double) light.angle, 0.1, 180.0) * (M_PI / 180.0) * 0.5);
    double outerScale   = std::max(computeConeScale(outerConeCos) * forwardDistance, 0.0001);
    double innerConeCos = cos(acos(Mth::clamp(outerConeCos, -1.0, 1.0)) * 0.7);
    double innerScale   = std::max(computeConeScale(innerConeCos) * forwardDistance, 0.0001);

    double localX = toSurface.dot(light.right);
    double localY = toSurface.dot(light.up);
    double width  = std::max((double) light.width, 0.01);
    double height = std::max((double) light.height, 0.01);

    double normalizedX = localX / (width * outerScale);
    double normalizedY = localY / (height * outerScale);
    double metric      = sqrt(normalizedX * normalizedX + normalizedY * normalizedY);

    double innerThreshold = clamp01(innerScale / outerScale);
    double fadeStart      = clamp01(innerThreshold - (1.0 - innerThreshold) * 0.75);
    return 1.0 - smoothstep(fadeStart, 1.0, metric);
}

static void pushCloudVertex(std::vector<float> *vertices, float x, float y, float z, float r,
                            float g, float b, float a)
{
    vertices->push_back(x);
    vertices->push_back(y);
    vertices->push_back(z);
    vertices->push_back(r);
    vertices->push_back(g);
    vertices->push_back(b);
    vertices->push_back(a);
}

static void pushCloudQuad(std::vector<float> *vertices, float x1, float y1, float z1, float x2,
                          float y2, float z2, float x3, float y3, float z3, float x4, float y4,
                          float z4, float r, float g, float b, float a)
{
    pushCloudVertex(vertices, x1, y1, z1, r, g, b, a);
    pushCloudVertex(vertices, x2, y2, z2, r, g, b, a);
    pushCloudVertex(vertices, x3, y3, z3, r, g, b, a);
    pushCloudVertex(vertices, x1, y1, z1, r, g, b, a);
    pushCloudVertex(vertices, x3, y3, z3, r, g, b, a);
    pushCloudVertex(vertices, x4, y4, z4, r, g, b, a);
}

LevelRenderer::LevelRenderer(Level *level, int width, int height)
    : m_levelShader(nullptr), m_skyShader(nullptr), m_cloudShader(nullptr), m_fogShader(nullptr),
      m_starShader(nullptr), m_starVao(0), m_starVbo(0), m_starVertexCount(0),
      m_sceneFramebuffer(nullptr), m_dynamicLightPipeline(nullptr), m_level(level),
      m_renderChunkGrid(false), m_cloudOffset(0.0f), m_cloudLastCamCellX(INT_MIN),
      m_cloudLastCamCellZ(INT_MIN), m_cloudLastBuiltOffset(-99999.0f), m_cloudLightSmooth(1.0f),
      m_lastSkyLightClamp(255), m_lightingMode(LightingMode::NEW), m_grassSideOverlayEnabled(true),
      m_blockOutlineMode(BlockOutlineMode::NORMAL)
{
    m_levelShader = new Shader("shaders/level.vert", "shaders/level.frag");
    m_skyShader   = new Shader("shaders/sky.vert", "shaders/sky.frag");
    m_cloudShader = new Shader("shaders/cloud.vert", "shaders/cloud.frag");
    m_fogShader   = new Shader("shaders/fog.vert", "shaders/fog.frag");
    m_starShader  = new Shader("shaders/stars.vert", "shaders/stars.frag");

    std::vector<float> stars = buildStarVertices();
    m_starVertexCount        = (int) (stars.size() / 3);

    m_starVao = RenderCommand::createVertexArray();
    m_starVbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_starVao);
    RenderCommand::bindArrayBuffer(m_starVbo);
    RenderCommand::uploadArrayBuffer(stars.data(), (uint32_t) (stars.size() * sizeof(float)),
                                     GL_STATIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * (uint32_t) sizeof(float), 0);

    RenderCommand::bindArrayBuffer(0);
    RenderCommand::bindVertexArray(0);

    m_sceneFramebuffer     = new Framebuffer(width, height);
    m_dynamicLightPipeline = new DynamicLightPipeline(width, height);

    ColormapManager *colormapManager = ColormapManager::getInstance();
    colormapManager->load("fog", "textures/colormap/fog.png");
    colormapManager->load("sky", "textures/colormap/sky.png");
    colormapManager->load("foliage", "textures/colormap/foliage.png");

    m_mesherRunning      = true;
    size_t mesherThreads = (size_t) std::max(1u, std::thread::hardware_concurrency() - 2);
    m_mesherPool         = std::make_unique<ThreadPool>(mesherThreads);
    m_maxMesherTasks     = std::max((size_t) 1, (mesherThreads + 1) / 2);

    float skyLightClamp = LightSource::sampleSkyLightClamp(m_level->getDimensionTime());
    m_lightStorage.reset(skyLightClamp);
    m_lightCache.rebuild(skyLightClamp);
}

LevelRenderer::~LevelRenderer()
{
    m_mesherRunning = false;
    if (m_mesherPool)
    {
        m_mesherPool->wait();
        m_mesherPool.reset();
    }

    if (m_sceneFramebuffer)
    {
        delete m_sceneFramebuffer;
        m_sceneFramebuffer = nullptr;
    }

    if (m_dynamicLightPipeline)
    {
        delete m_dynamicLightPipeline;
        m_dynamicLightPipeline = nullptr;
    }

    if (m_starVbo)
    {
        RenderCommand::deleteBuffer(m_starVbo);
    }
    if (m_starVao)
    {
        RenderCommand::deleteVertexArray(m_starVao);
    }

    if (m_starShader)
    {
        delete m_starShader;
        m_starShader = nullptr;
    }

    if (m_fogShader)
    {
        delete m_fogShader;
        m_fogShader = nullptr;
    }

    if (m_cloudShader)
    {
        delete m_cloudShader;
        m_cloudShader = nullptr;
    }

    if (m_skyShader)
    {
        delete m_skyShader;
        m_skyShader = nullptr;
    }

    if (m_levelShader)
    {
        delete m_levelShader;
        m_levelShader = nullptr;
    }
}

void LevelRenderer::onResize(int width, int height)
{
    m_sceneFramebuffer->resize(width, height);
    if (m_dynamicLightPipeline)
    {
        m_dynamicLightPipeline->resize(width, height);
    }
}

void LevelRenderer::render(float partialTicks)
{
    Minecraft *minecraft = Minecraft::getInstance();
    const Camera *camera = minecraft->getCamera();

    const Vec3 cameraPos = camera->getPosition();
    const Vec3 playerPos = minecraft->getLocalPlayer()->getPosition();
    Mat4 viewMatrix      = camera->getViewMatrix();
    Mat4 projection      = minecraft->getProjection();

    const DimensionTime &dimensionTime = m_level->getDimensionTime();

    float blendFactor = 1.0f - expf(-Time::getDelta() * 2.5f);
    float cloudTarget = dimensionTime.getDaylightFactor() * 0.90f + 0.10f;
    m_cloudLightSmooth += (cloudTarget - m_cloudLightSmooth) * blendFactor;
    updateLightState(dimensionTime);

    setupMatrices(cameraPos);
    setupFog();

    Tesselator::getInstance()->getBuilderForLevel()->setViewProjection(viewMatrix, projection);
    minecraft->getDefaultFont()->setNearest(true);
    minecraft->getDefaultFont()->setLevelViewProjection(viewMatrix, projection);

    m_sceneFramebuffer->bind();
    RenderCommand::clearAll();

    renderSky(viewMatrix, projection);
    renderStars(viewMatrix, projection);

    uploadPendingMeshes();
    scheduleMesher();
    if (m_lightingMode == LightingMode::OLD && Lighting::isOn())
    {
        beginSkyLightClampUpdate(dimensionTime);
        pumpSkyLightClampUpdate(64, 64);
    }

    std::vector<VisibleChunk> opaqueChunks;
    std::vector<VisibleChunk> fadingChunks;
    collectVisibleChunks(playerPos, cameraPos, viewMatrix, projection, &opaqueChunks,
                         &fadingChunks);

    bindLevelShader(cameraPos, viewMatrix, projection);
    renderChunks(m_levelShader, opaqueChunks, fadingChunks);
    renderRenderObjects(LevelRenderObjectStage::BEFORE_CLOUDS);
    renderBlockOutline();

    Framebuffer *activeSceneFramebuffer = m_sceneFramebuffer;
    m_forwardDynamicLights.clear();
    if (m_dynamicLightPipeline)
    {
        m_dynamicLightPipeline->prepareFrame(m_level, cameraPos);
        m_dynamicLightPipeline->copySubmittedLights(&m_forwardDynamicLights);
        if (m_dynamicLightPipeline->hasLightsToRender())
        {
            activeSceneFramebuffer->unbind();
            renderDynamicLightBuffers(cameraPos, viewMatrix, projection, opaqueChunks,
                                      fadingChunks);
            Framebuffer *litFramebuffer =
                    m_dynamicLightPipeline->apply(m_level, m_sceneFramebuffer);
            activeSceneFramebuffer = litFramebuffer ? litFramebuffer : m_sceneFramebuffer;
            activeSceneFramebuffer->bind();
        }
    }

    renderClouds(viewMatrix, projection);
    renderRenderObjects(LevelRenderObjectStage::AFTER_CLOUDS);

    renderEntities(partialTicks);
    renderRenderObjects(LevelRenderObjectStage::AFTER_ENTITIES);
    renderParticles(partialTicks);

    activeSceneFramebuffer->unbind();

    renderFogPass(projection, activeSceneFramebuffer ? activeSceneFramebuffer : m_sceneFramebuffer);
    renderRenderObjects(LevelRenderObjectStage::AFTER_FOG);
    renderEntityNameTags(partialTicks);

    if (m_renderChunkGrid)
    {
        renderChunkGrid();
    }

    GlStateManager::popMatrix();
}

void LevelRenderer::cycleLightingMode()
{
    if (m_lightingMode == LightingMode::NEW)
    {
        m_lightingMode = LightingMode::OLD;
        Lighting::turnOn();
    }
    else if (m_lightingMode == LightingMode::OLD)
    {
        m_lightingMode = LightingMode::OFF;
        Lighting::turnOff();
    }
    else
    {
        m_lightingMode = LightingMode::NEW;
        Lighting::turnOn();
    }

    float skyLightClamp =
            m_lightingMode == LightingMode::NEW
                    ? LightSource::sampleSkyLightClamp(m_level->getDimensionTime())
                    : (m_lightingMode == LightingMode::OLD ? (float) m_level->getSkyLightClamp()
                                                           : 15.0f);
    m_lightStorage.reset(skyLightClamp);
    m_lightCache.rebuild(skyLightClamp);
    m_lastSkyLightClamp = 255;
    m_skyQueue.clear();
    m_skyQueued.clear();
    {
        std::lock_guard<std::mutex> lock(m_skyMutex);
        m_skyResults.clear();
    }

    Logger::logInfo("Lighting mode: %s",
                    m_lightingMode == LightingMode::NEW
                            ? "new"
                            : (m_lightingMode == LightingMode::OLD ? "old" : "off"));

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks =
            m_level->getChunks();
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>::const_iterator chunkIt;
    for (chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt)
    {
        const ChunkPos &pos                 = chunkIt->first;
        const std::unique_ptr<Chunk> &chunk = chunkIt->second;
        (void) chunk;
        rebuildChunkUrgent(pos);
    }
}

LevelRenderer::LightingMode LevelRenderer::getLightingMode() const { return m_lightingMode; }

void LevelRenderer::cycleBlockOutlineMode()
{
    if (m_blockOutlineMode == BlockOutlineMode::NORMAL)
    {
        m_blockOutlineMode = BlockOutlineMode::BEDROCK;
    }
    else
    {
        m_blockOutlineMode = BlockOutlineMode::NORMAL;
    }

    const char *mode = m_blockOutlineMode == BlockOutlineMode::BEDROCK ? "bedrock" : "normal";
    Logger::logInfo("Block outline mode: %s", mode);
}

LevelRenderer::BlockOutlineMode LevelRenderer::getBlockOutlineMode() const
{
    return m_blockOutlineMode;
}

void LevelRenderer::toggleGrassSideOverlay()
{
    m_grassSideOverlayEnabled = !m_grassSideOverlayEnabled;
    Logger::logInfo("Grass side overlay: %s", m_grassSideOverlayEnabled ? "on" : "off");

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks =
            m_level->getChunks();
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>::const_iterator chunkIt;
    for (chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt)
    {
        const ChunkPos &pos                 = chunkIt->first;
        const std::unique_ptr<Chunk> &chunk = chunkIt->second;
        (void) chunk;
        rebuildChunkUrgent(pos);
    }
}

bool LevelRenderer::isGrassSideOverlayEnabled() const { return m_grassSideOverlayEnabled; }

size_t LevelRenderer::getMeshedChunkCount() const { return m_chunks.size(); }

size_t LevelRenderer::getVisibleChunkCount() const { return m_lastVisibleChunkCount; }

size_t LevelRenderer::getRenderedMeshCount() const { return m_lastRenderedMeshCount; }

size_t LevelRenderer::getSkyQueueCount() const { return m_skyQueue.size(); }

size_t LevelRenderer::getMesherThreadCount() const
{
    if (!m_mesherPool)
    {
        return 0;
    }
    return m_mesherPool->getThreadCount();
}

void LevelRenderer::updateLightState(const DimensionTime &dimensionTime)
{
    float skyLightClamp = m_lightingMode == LightingMode::NEW
                                  ? LightSource::sampleSkyLightClamp(dimensionTime)
                                  : (m_lightingMode == LightingMode::OLD
                                             ? (float) dimensionTime.getSkyLightClamp()
                                             : 15.0f);
    m_lightStorage.update(skyLightClamp, Time::getDelta(), m_lightingMode == LightingMode::NEW);
    m_lightCache.rebuild(m_lightStorage.getSkyLightClamp());
}
void LevelRenderer::bindLevelShader(const Vec3 &cameraPos, const Mat4 &viewMatrix,
                                    const Mat4 &projection)
{
    m_levelShader->bind();
    m_levelShader->setInt("u_texture", 0);
    m_levelShader->setInt("u_grassOverlayTexture", 1);
    m_levelShader->setInt("u_grassSideOverlayEnabled", m_grassSideOverlayEnabled ? 1 : 0);
    int lightingMode =
            m_lightingMode == LightingMode::NEW ? 1 : (m_lightingMode == LightingMode::OFF ? 2 : 0);
    m_levelShader->setInt("u_lightingMode", lightingMode);

    const std::array<float, 16> &skyLightLevels = m_lightCache.getSkyLightLevels();
    for (int i = 0; i < 16; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "u_skyLightLevels[%d]", i);
        m_levelShader->setFloat(name, skyLightLevels[(unsigned int) i]);
    }

    Texture *grassOverlayMask = BlockRegistry::getTextureRepository()
                                        ->get("textures/block/grass_side_overlay.png")
                                        .get();
    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(grassOverlayMask ? grassOverlayMask->getId() : 0);
    RenderCommand::activeTexture(0);

    m_levelShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_levelShader->setMat4("u_view", viewMatrix.data);
    m_levelShader->setMat4("u_projection", projection.data);
    m_levelShader->setVec3("u_cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
    m_levelShader->setVec3("u_chunkOffset", 0.0f, 0.0f, 0.0f);
    m_levelShader->setFloat("u_chunkAlpha", 1.0f);

    float fogR;
    float fogG;
    float fogB;
    GlStateManager::getFogColor(&fogR, &fogG, &fogB);
    m_levelShader->setVec3("u_chunkFadeColor", fogR, fogG, fogB);

    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    GlStateManager::enableDepthTest();
}

void LevelRenderer::collectVisibleChunks(const Vec3 &playerPos, const Vec3 &cameraPos,
                                         const Mat4 &viewMatrix, const Mat4 &projection,
                                         std::vector<VisibleChunk> *opaqueChunks,
                                         std::vector<VisibleChunk> *fadingChunks)
{
    int playerChunkX   = Mth::floorDiv((int) playerPos.x, Chunk::SIZE_X);
    int playerChunkZ   = Mth::floorDiv((int) playerPos.z, Chunk::SIZE_Z);
    int renderDistance = m_level->getRenderDistance();
    float delta        = (float) Time::getDelta();

    Mat4 viewProjection = projection.multiply(viewMatrix).multiply(GlStateManager::getMatrix());
    m_frustum.extractPlanes(viewProjection);

    opaqueChunks->clear();
    fadingChunks->clear();
    opaqueChunks->reserve(m_chunks.size());
    fadingChunks->reserve(m_chunks.size());

    size_t visibleChunkCount = 0;
    size_t renderedMeshCount = 0;

    std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>,
                       ChunkPosHash>::const_iterator chunkIt;
    for (chunkIt = m_chunks.begin(); chunkIt != m_chunks.end(); ++chunkIt)
    {
        const ChunkPos &pos                                   = chunkIt->first;
        const std::vector<std::unique_ptr<ChunkMesh>> &meshes = chunkIt->second;
        int dx                                                = pos.x - playerChunkX;
        int dz                                                = pos.z - playerChunkZ;
        if (dx * dx + dz * dz > renderDistance * renderDistance)
        {
            tickHiddenChunkFadeState(pos, delta);
            continue;
        }

        Vec3 chunkMin(pos.x * Chunk::SIZE_X, pos.y * Chunk::SIZE_Y, pos.z * Chunk::SIZE_Z);
        Vec3 chunkMax(chunkMin.x + Chunk::SIZE_X, chunkMin.y + Chunk::SIZE_Y,
                      chunkMin.z + Chunk::SIZE_Z);
        AABB chunkAABB(chunkMin, chunkMax);
        if (!m_frustum.testAABB(chunkAABB))
        {
            tickHiddenChunkFadeState(pos, 0.0f);
            continue;
        }

        visibleChunkCount++;
        renderedMeshCount += meshes.size();
        float alpha    = getChunkFadeAlpha(pos, delta);
        double centerX = chunkMin.x + Chunk::SIZE_X * 0.5;
        double centerY = chunkMin.y + Chunk::SIZE_Y * 0.5;
        double centerZ = chunkMin.z + Chunk::SIZE_Z * 0.5;
        double distX   = centerX - cameraPos.x;
        double distY   = centerY - cameraPos.y;
        double distZ   = centerZ - cameraPos.z;

        VisibleChunk entry{&pos, &meshes, alpha, distX * distX + distY * distY + distZ * distZ};
        if (alpha >= 0.999f)
        {
            opaqueChunks->push_back(entry);
        }
        else
        {
            fadingChunks->push_back(entry);
        }
    }

    m_lastVisibleChunkCount = visibleChunkCount;
    m_lastRenderedMeshCount = renderedMeshCount;
}

void LevelRenderer::renderChunks(Shader *shader, const std::vector<VisibleChunk> &opaqueChunks,
                                 std::vector<VisibleChunk> fadingChunks)
{
    if (!shader)
    {
        return;
    }

    GlStateManager::disableBlend();
    shader->setFloat("u_chunkAlpha", 1.0f);

    for (const VisibleChunk &entry : opaqueChunks)
    {
        const ChunkPos &pos = *entry.pos;
        shader->setVec3("u_chunkOffset", (float) (pos.x * Chunk::SIZE_X),
                        (float) (pos.y * Chunk::SIZE_Y), (float) (pos.z * Chunk::SIZE_Z));
        for (const std::unique_ptr<ChunkMesh> &mesh : *entry.meshes)
        {
            mesh->render();
        }
    }

    if (!fadingChunks.empty())
    {
        std::sort(fadingChunks.begin(), fadingChunks.end(),
                  [](const VisibleChunk &a, const VisibleChunk &b) {
                      return a.distanceSquared > b.distanceSquared;
                  });

        for (const VisibleChunk &entry : fadingChunks)
        {
            const ChunkPos &pos = *entry.pos;
            shader->setVec3("u_chunkOffset", (float) (pos.x * Chunk::SIZE_X),
                            (float) (pos.y * Chunk::SIZE_Y), (float) (pos.z * Chunk::SIZE_Z));
            shader->setFloat("u_chunkAlpha", entry.alpha);
            for (const std::unique_ptr<ChunkMesh> &mesh : *entry.meshes)
            {
                mesh->render();
            }
        }

        shader->setFloat("u_chunkAlpha", 1.0f);
    }
}

void LevelRenderer::renderDynamicLightBuffers(const Vec3 &cameraPos, const Mat4 &viewMatrix,
                                              const Mat4 &projection,
                                              const std::vector<VisibleChunk> &opaqueChunks,
                                              const std::vector<VisibleChunk> &fadingChunks)
{
    if (!m_dynamicLightPipeline)
    {
        return;
    }

    float fogR;
    float fogG;
    float fogB;
    GlStateManager::getFogColor(&fogR, &fogG, &fogB);

    if (Shader *albedoShader = m_dynamicLightPipeline->beginAlbedoCapture(
                cameraPos, viewMatrix, projection, m_grassSideOverlayEnabled,
                Vec3((double) fogR, (double) fogG, (double) fogB)))
    {
        renderChunks(albedoShader, opaqueChunks, fadingChunks);
    }

    if (Shader *positionShader =
                m_dynamicLightPipeline->beginPositionCapture(viewMatrix, projection))
    {
        renderChunks(positionShader, opaqueChunks, fadingChunks);
    }

    if (Shader *normalShader = m_dynamicLightPipeline->beginNormalCapture(viewMatrix, projection))
    {
        renderChunks(normalShader, opaqueChunks, fadingChunks);
    }
}

void LevelRenderer::renderChunkGrid() const
{
    GlStateManager::disableDepthTest();

    BufferBuilder *renderer = Tesselator::getInstance()->getBuilderForLevel();
    renderer->begin(GL_LINES);
    renderer->color(0xFF00FF00);

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks =
            m_level->getChunks();
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>::const_iterator chunkIt;
    for (chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt)
    {
        const ChunkPos &pos = chunkIt->first;
        float minX          = (float) (pos.x * Chunk::SIZE_X);
        float minY          = (float) (pos.y * Chunk::SIZE_Y);
        float minZ          = (float) (pos.z * Chunk::SIZE_Z);

        float maxX = minX + Chunk::SIZE_X;
        float maxY = minY + Chunk::SIZE_Y;
        float maxZ = minZ + Chunk::SIZE_Z;

        renderer->vertex(minX, minY, minZ);
        renderer->vertex(maxX, minY, minZ);

        renderer->vertex(maxX, minY, minZ);
        renderer->vertex(maxX, minY, maxZ);

        renderer->vertex(maxX, minY, maxZ);
        renderer->vertex(minX, minY, maxZ);

        renderer->vertex(minX, minY, maxZ);
        renderer->vertex(minX, minY, minZ);

        renderer->vertex(minX, maxY, minZ);
        renderer->vertex(maxX, maxY, minZ);

        renderer->vertex(maxX, maxY, minZ);
        renderer->vertex(maxX, maxY, maxZ);

        renderer->vertex(maxX, maxY, maxZ);
        renderer->vertex(minX, maxY, maxZ);

        renderer->vertex(minX, maxY, maxZ);
        renderer->vertex(minX, maxY, minZ);

        renderer->vertex(minX, minY, minZ);
        renderer->vertex(minX, maxY, minZ);

        renderer->vertex(maxX, minY, minZ);
        renderer->vertex(maxX, maxY, minZ);

        renderer->vertex(maxX, minY, maxZ);
        renderer->vertex(maxX, maxY, maxZ);

        renderer->vertex(minX, minY, maxZ);
        renderer->vertex(minX, maxY, maxZ);
    }

    renderer->end();
}

void LevelRenderer::renderParticles(float partialTicks)
{
    constexpr double particleRenderDistance   = 32.0;
    constexpr double particleRenderDistanceSq = particleRenderDistance * particleRenderDistance;
    constexpr double particleFrustumRadiusScale = 1.41421356237;
    constexpr float particleDepthAlphaCutoff = 0.1f;

    ParticleEngine *particleEngine = m_level->getParticleEngine();
    if (!particleEngine)
    {
        return;
    }

    Texture *texture = ParticleRegistry::getTexture();
    if (!texture)
    {
        return;
    }

    std::vector<ParticleEngine::RenderParticle> particles;
    particleEngine->copyRenderParticles(&particles);
    if (particles.empty())
    {
        return;
    }

    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 cameraPos = minecraft->getCamera()->getPosition();
    Vec3 front           = minecraft->getLocalPlayer()->getFront().normalize();
    Vec3 worldUp         = fabs(front.y) > 0.99 ? Vec3(0.0, 0.0, 1.0) : Vec3(0.0, 1.0, 0.0);
    Vec3 right           = front.cross(worldUp).normalize();
    Vec3 up              = right.cross(front).normalize();

    struct VisibleParticle
    {
        TextureAtlas::Region region;
        Vec3 pos;
        uint32_t color;
        float size;
    };

    std::vector<VisibleParticle> visibleParticles;
    visibleParticles.reserve(particles.size());

    for (const ParticleEngine::RenderParticle &particle : particles)
    {
        if (!ParticleRegistry::hasAtlasRegion(particle.type))
        {
            continue;
        }

        Vec3 pos = particle.oldPosition.add(
                particle.position.sub(particle.oldPosition).scale((double) partialTicks));
        if (pos.distanceSquared(cameraPos) > particleRenderDistanceSq)
        {
            continue;
        }
        if (!m_frustum.testSphere(pos,
                                  std::max((double) particle.size * particleFrustumRadiusScale,
                                           0.05)))
        {
            continue;
        }

        uint32_t color = particle.color;
        if (!particle.lit)
        {
            color = multiplyColorRgb(color, sampleWorldLightColor(pos));
        }

        visibleParticles.push_back(
                {ParticleRegistry::getAtlasRegion(particle.type), pos, color, particle.size});
    }

    if (visibleParticles.empty())
    {
        return;
    }

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForLevel();
    auto emitParticles = [&](bool depthOnly) {
        builder->begin(GL_TRIANGLES);
        builder->bindTexture(texture);
        builder->setAlphaCutoff(depthOnly ? particleDepthAlphaCutoff : -1.0f);

        for (const VisibleParticle &particle : visibleParticles)
        {
            const Vec3 &pos = particle.pos;
            uint32_t color  = depthOnly ? 0xFFFFFFFFu : particle.color;
            Vec3 dx = right.scale((double) particle.size);
            Vec3 dy = up.scale((double) particle.size);

            Vec3 p1 = pos.sub(dx).sub(dy);
            Vec3 p2 = pos.sub(dx).add(dy);
            Vec3 p3 = pos.add(dx).add(dy);
            Vec3 p4 = pos.add(dx).sub(dy);

            builder->color(color);
            builder->vertexUV((float) p1.x, (float) p1.y, (float) p1.z, particle.region.u0,
                              particle.region.v1);
            builder->vertexUV((float) p2.x, (float) p2.y, (float) p2.z, particle.region.u0,
                              particle.region.v0);
            builder->vertexUV((float) p3.x, (float) p3.y, (float) p3.z, particle.region.u1,
                              particle.region.v0);
            builder->vertexUV((float) p1.x, (float) p1.y, (float) p1.z, particle.region.u0,
                              particle.region.v1);
            builder->vertexUV((float) p3.x, (float) p3.y, (float) p3.z, particle.region.u1,
                              particle.region.v0);
            builder->vertexUV((float) p4.x, (float) p4.y, (float) p4.z, particle.region.u1,
                              particle.region.v1);
        }

        builder->end();
        builder->unbindTexture();
        builder->setAlphaCutoff(-1.0f);
    };

    GlStateManager::enableDepthTest();
    GlStateManager::disableCull();

    GlStateManager::disableBlend();
    GlStateManager::setDepthMask(true);
    GlStateManager::setColorMask(false, false, false, false);
    emitParticles(true);

    GlStateManager::setColorMask(true, true, true, true);
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GlStateManager::setDepthMask(false);
    GlStateManager::setDepthFunc(GL_EQUAL);
    emitParticles(false);

    GlStateManager::setDepthFunc(GL_LEQUAL);
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
}

void LevelRenderer::renderBlockOutline()
{
    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 pos       = minecraft->getCamera()->getPosition();
    const Vec3 direction = minecraft->getLocalPlayer()->getFront().normalize();

    HitResult *hit = m_level->clip(pos, direction, 6.0f);
    if (!hit)
    {
        return;
    }

    if (!hit->isBlock())
    {
        delete hit;
        return;
    }

    const BlockPos blockPos = hit->getBlockPos();
    delete hit;

    constexpr float outlineEpsilon = 0.0035f;
    constexpr float overlayEpsilon = 0.00075f;

    Block *block = Block::byId(m_level->getBlockId(blockPos));
    if (!block)
    {
        return;
    }

    const Chunk *chunk = m_level->getChunk(ChunkPos(Mth::floorDiv(blockPos.x, Chunk::SIZE_X),
                                                    Mth::floorDiv(blockPos.y, Chunk::SIZE_Y),
                                                    Mth::floorDiv(blockPos.z, Chunk::SIZE_Z)));
    if (!chunk)
    {
        return;
    }

    int lx        = Mth::floorMod(blockPos.x, Chunk::SIZE_X);
    int ly        = Mth::floorMod(blockPos.y, Chunk::SIZE_Y);
    int lz        = Mth::floorMod(blockPos.z, Chunk::SIZE_Z);
    AABB blockBox = block->getPlacedAABB(
            blockPos, decodeDirection(chunk->getBlockAttachmentFace(lx, ly, lz)));
    const Vec3 &blockMin = blockBox.getMin();
    const Vec3 &blockMax = blockBox.getMax();

    float overlayMinX = (float) blockMin.x - overlayEpsilon;
    float overlayMinY = (float) blockMin.y - overlayEpsilon;
    float overlayMinZ = (float) blockMin.z - overlayEpsilon;
    float overlayMaxX = (float) blockMax.x + overlayEpsilon;
    float overlayMaxY = (float) blockMax.y + overlayEpsilon;
    float overlayMaxZ = (float) blockMax.z + overlayEpsilon;

    float minX = (float) blockMin.x - outlineEpsilon;
    float minY = (float) blockMin.y - outlineEpsilon;
    float minZ = (float) blockMin.z - outlineEpsilon;
    float maxX = (float) blockMax.x + outlineEpsilon;
    float maxY = (float) blockMax.y + outlineEpsilon;
    float maxZ = (float) blockMax.z + outlineEpsilon;

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForLevel();

    if (m_blockOutlineMode == BlockOutlineMode::BEDROCK)
    {
        GlStateManager::enableBlend();
        GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        builder->begin(GL_TRIANGLES);
        builder->color(0x55FFFFFF);

        builder->vertex(overlayMinX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMinZ);

        builder->vertex(overlayMaxX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMaxZ);

        builder->vertex(overlayMinX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMaxZ);

        builder->vertex(overlayMaxX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMinZ);

        builder->vertex(overlayMinX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMinZ);
        builder->vertex(overlayMaxX, overlayMaxY, overlayMaxZ);
        builder->vertex(overlayMinX, overlayMaxY, overlayMaxZ);

        builder->vertex(overlayMinX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMaxZ);
        builder->vertex(overlayMaxX, overlayMinY, overlayMinZ);
        builder->vertex(overlayMinX, overlayMinY, overlayMinZ);

        builder->end();

        GlStateManager::disableBlend();
        GlStateManager::setDepthMask(true);
        GlStateManager::enableCull();
        return;
    }

    RenderCommand::setLineWidth(2.0f);

    builder->begin(GL_LINES);
    builder->color(0xFF000000);

    builder->vertex(minX, minY, minZ);
    builder->vertex(maxX, minY, minZ);
    builder->vertex(maxX, minY, minZ);
    builder->vertex(maxX, minY, maxZ);
    builder->vertex(maxX, minY, maxZ);
    builder->vertex(minX, minY, maxZ);
    builder->vertex(minX, minY, maxZ);
    builder->vertex(minX, minY, minZ);

    builder->vertex(minX, maxY, minZ);
    builder->vertex(maxX, maxY, minZ);
    builder->vertex(maxX, maxY, minZ);
    builder->vertex(maxX, maxY, maxZ);
    builder->vertex(maxX, maxY, maxZ);
    builder->vertex(minX, maxY, maxZ);
    builder->vertex(minX, maxY, maxZ);
    builder->vertex(minX, maxY, minZ);

    builder->vertex(minX, minY, minZ);
    builder->vertex(minX, maxY, minZ);
    builder->vertex(maxX, minY, minZ);
    builder->vertex(maxX, maxY, minZ);
    builder->vertex(maxX, minY, maxZ);
    builder->vertex(maxX, maxY, maxZ);
    builder->vertex(minX, minY, maxZ);
    builder->vertex(minX, maxY, maxZ);

    builder->end();

    GlStateManager::setDepthMask(true);
    GlStateManager::enableCull();
}

float LevelRenderer::getChunkFadeAlpha(const ChunkPos &pos, float delta)
{
    ChunkFadeState &fadeState = m_chunkFadeStates[pos];

    if (!fadeState.visible)
    {
        if (fadeState.pendingFade || !fadeState.hasRendered ||
            fadeState.hiddenTime >= m_chunkFadeSettings.reappearDelay)
        {
            fadeState.alpha = 0.0f;
        }
    }

    fadeState.visible     = true;
    fadeState.hiddenTime  = 0.0f;
    fadeState.pendingFade = false;

    if (m_chunkFadeSettings.fadeInDuration <= 0.0f)
    {
        fadeState.alpha = 1.0f;
    }
    else
    {
        fadeState.alpha = Mth::clampf(fadeState.alpha + delta / m_chunkFadeSettings.fadeInDuration,
                                      0.0f, 1.0f);
    }

    fadeState.hasRendered = true;
    return fadeState.alpha;
}

void LevelRenderer::tickHiddenChunkFadeState(const ChunkPos &pos, float delta)
{
    std::unordered_map<ChunkPos, ChunkFadeState, ChunkPosHash>::iterator it =
            m_chunkFadeStates.find(pos);
    if (it == m_chunkFadeStates.end())
    {
        return;
    }

    ChunkFadeState &fadeState = it->second;
    if (fadeState.visible)
    {
        fadeState.visible    = false;
        fadeState.hiddenTime = 0.0f;
    }
    else
    {
        fadeState.hiddenTime += delta;
    }
}

void LevelRenderer::renderEntities(float partialTicks)
{
    constexpr double entityRenderDistance   = 256.0;
    constexpr double entityRenderDistanceSq = entityRenderDistance * entityRenderDistance;

    RenderCommand::setColorMask(true, true, true, true);
    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::enableCull();

    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 cameraPos = minecraft->getCamera()->getPosition();

    for (const std::unique_ptr<Entity> &entity : m_level->getEntities())
    {
        Vec3 renderPos = entity->getRenderPosition(partialTicks);
        if (renderPos.distanceSquared(cameraPos) > entityRenderDistanceSq)
        {
            continue;
        }

        uint32_t lightColor      = sampleEntityLightColor(entity.get(), partialTicks);
        EntityRenderer *renderer = EntityRendererRegistry::get()->getValue(entity->getType());
        renderer->render(entity.get(), partialTicks, lightColor);
    }
}

void LevelRenderer::renderEntityNameTags(float partialTicks)
{
    constexpr double entityRenderDistance   = 256.0;
    constexpr double entityRenderDistanceSq = entityRenderDistance * entityRenderDistance;

    Minecraft *minecraft = Minecraft::getInstance();
    Font *font           = minecraft->getDefaultFont();
    const Vec3 cameraPos = minecraft->getCamera()->getPosition();

    for (const std::unique_ptr<Entity> &entity : m_level->getEntities())
    {
        Vec3 pos = entity->getRenderPosition(partialTicks);
        if (pos.distanceSquared(cameraPos) > entityRenderDistanceSq)
        {
            continue;
        }

        std::string strUuid = entity->getUUID().toString();
        std::wstring wstrUuid(strUuid.begin(), strUuid.end());
        if (wstrUuid.empty())
        {
            continue;
        }

        AABB box     = entity->getBoundingBox().translated(pos);
        float height = box.getMax().y - box.getMin().y;

        Vec3 textPos = pos.add(Vec3(0.0, height + 0.5, 0.0));

        GlStateManager::disableDepthTest();
        font->levelRenderShadow(wstrUuid, textPos, 0.015f, 0xFFFFFFFF);
    }
}

uint32_t LevelRenderer::sampleEntityLightColor(const Entity *entity, float partialTicks) const
{
    if (!entity || m_lightingMode == LightingMode::OFF || !Lighting::isOn())
    {
        return 0xFFFFFFFF;
    }

    Vec3 pos        = entity->getRenderPosition(partialTicks);
    AABB box        = entity->getBoundingBox().translated(pos);
    const Vec3 &min = box.getMin();
    const Vec3 &max = box.getMax();
    Vec3 samplePos((min.x + max.x) * 0.5, min.y + (max.y - min.y) * 0.6, (min.z + max.z) * 0.5);

    return sampleWorldLightColor(samplePos);
}

Vec3 LevelRenderer::sampleDynamicLightRgb(const Vec3 &samplePos) const
{
    if (!m_dynamicLightPipeline || !m_dynamicLightPipeline->isEnabled() ||
        m_forwardDynamicLights.empty())
    {
        return Vec3(0.0, 0.0, 0.0);
    }

    Vec3 dynamicLight(0.0, 0.0, 0.0);

    for (const DynamicLightPipeline::PackedLight &light : m_forwardDynamicLights)
    {
        Vec3 lightColor(std::max(0.0, light.color.x), std::max(0.0, light.color.y),
                        std::max(0.0, light.color.z));
        double brightness = std::max(0.0, (double) light.brightness);
        if (brightness <= 0.0)
        {
            continue;
        }

        if (light.type == DynamicLightPipeline::PackedLightType::DIRECTIONAL)
        {
            double diffuse = smoothstep(-0.2, 0.2, -light.direction.y);
            if (diffuse <= 0.0)
            {
                continue;
            }

            dynamicLight = dynamicLight.add(lightColor.scale(brightness * diffuse));
            continue;
        }

        Vec3 offset          = light.position.sub(samplePos);
        double distanceSq    = offset.lengthSquared();
        double maxDistance   = light.type == DynamicLightPipeline::PackedLightType::POINT
                                       ? std::max(0.1, (double) light.radius)
                                       : std::max(0.1, (double) light.distance);
        double maxDistanceSq = maxDistance * maxDistance;
        if (distanceSq >= maxDistanceSq)
        {
            continue;
        }

        double attenuation = attenuateNoCusp(sqrt(distanceSq), maxDistance);
        if (attenuation <= 0.0)
        {
            continue;
        }

        if (light.type == DynamicLightPipeline::PackedLightType::AREA)
        {
            double spotMask = computeAreaSpotMask(light, samplePos.sub(light.position));
            if (spotMask <= 0.0)
            {
                continue;
            }

            dynamicLight = dynamicLight.add(lightColor.scale(brightness * attenuation * spotMask));
            continue;
        }

        dynamicLight = dynamicLight.add(lightColor.scale(brightness * attenuation));
    }

    return dynamicLight;
}

uint32_t LevelRenderer::sampleWorldLightColor(const Vec3 &samplePos) const
{
    if (m_lightingMode == LightingMode::OFF || !Lighting::isOn())
    {
        return 0xFFFFFFFF;
    }

    BlockPos lightPos((int) floor(samplePos.x), (int) floor(samplePos.y), (int) floor(samplePos.z));

    uint8_t blockR;
    uint8_t blockG;
    uint8_t blockB;
    LightEngine::getBlockLight(m_level, lightPos, &blockR, &blockG, &blockB);

    uint8_t sky = m_level->getSkyLightLevel(lightPos);
    uint8_t skyClamp =
            (uint8_t) (Mth::clampf(m_lightStorage.getSkyLightClamp(), 0.0f, 15.0f) + 0.5f);
    if (sky > skyClamp)
    {
        sky = skyClamp;
    }

    float minLight    = 0.03f;
    Vec3 dynamicLight = sampleDynamicLightRgb(samplePos);
    float r           = (float) (blockR > sky ? blockR : sky) / 15.0f + (float) dynamicLight.x;
    float g           = (float) (blockG > sky ? blockG : sky) / 15.0f + (float) dynamicLight.y;
    float b           = (float) (blockB > sky ? blockB : sky) / 15.0f + (float) dynamicLight.z;

    if (r < minLight)
    {
        r = minLight;
    }
    if (g < minLight)
    {
        g = minLight;
    }
    if (b < minLight)
    {
        b = minLight;
    }

    uint32_t ir = (uint32_t) (Mth::clampf(r, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t ig = (uint32_t) (Mth::clampf(g, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t ib = (uint32_t) (Mth::clampf(b, 0.0f, 1.0f) * 255.0f + 0.5f);
    return 0xFF000000u | (ir << 16) | (ig << 8) | ib;
}

void LevelRenderer::renderFogPass(const Mat4 &projection, Framebuffer *sourceFramebuffer)
{
    GlStateManager::disableDepthTest();

    if (!sourceFramebuffer)
    {
        sourceFramebuffer = m_sceneFramebuffer;
    }

    m_fogShader->bind();

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(sourceFramebuffer->getColorTexture());
    m_fogShader->setInt("u_colorTexture", 0);

    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(sourceFramebuffer->getDepthTexture());
    m_fogShader->setInt("u_depthTexture", 1);

    m_fogShader->setInt("u_fogColormap", 2);
    ColormapManager::getInstance()->bind("fog", 2);

    Fog &fog = m_level->getFog();
    m_fogShader->setVec2("u_fogLut", fog.getLutX(), fog.getLutY());
    m_fogShader->setFloat("u_dayFactor", m_level->getDaylightFactor());

    bool fogEnabled = GlStateManager::isFogEnabled();
    m_fogShader->setInt("u_fogEnabled", fogEnabled);

    m_fogShader->setFloat("u_fogStrength", 1.0f);

    if (fogEnabled)
    {
        float fogStart;
        float fogEnd;
        GlStateManager::getFogRange(&fogStart, &fogEnd);
        m_fogShader->setVec2("u_fogRange", fogStart, fogEnd);
    }

    Minecraft *minecraft        = Minecraft::getInstance();
    Mat4 viewMatrix             = minecraft->getCamera()->getViewMatrix();
    Mat4 invertedViewProjection = projection.multiply(viewMatrix).inverse();
    m_fogShader->setMat4("u_invViewProj", invertedViewProjection.data);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);

    GlStateManager::enableDepthTest();
}

void LevelRenderer::renderDynamicLightImGui()
{
    if (m_dynamicLightPipeline)
    {
        m_dynamicLightPipeline->renderImGui(m_level);
    }
}

void LevelRenderer::toggleDynamicLightPipeline()
{
    if (m_dynamicLightPipeline)
    {
        m_dynamicLightPipeline->toggleEnabled();
    }
}

bool LevelRenderer::isDynamicLightPipelineEnabled() const
{
    return m_dynamicLightPipeline && m_dynamicLightPipeline->isEnabled();
}

void LevelRenderer::beginSkyLightClampUpdate(const DimensionTime &dimensionTime)
{
    uint8_t clamp = dimensionTime.getSkyLightClamp();
    if (clamp == m_lastSkyLightClamp)
    {
        return;
    }

    m_lastSkyLightClamp = clamp;
    m_skyClampTarget    = clamp;

    m_skyQueue.clear();
    m_skyQueued.clear();

    Minecraft *minecraft = Minecraft::getInstance();
    Vec3 playerPos       = minecraft->getLocalPlayer()->getPosition();

    int pcx = Mth::floorDiv((int) playerPos.x, Chunk::SIZE_X);
    int pcz = Mth::floorDiv((int) playerPos.z, Chunk::SIZE_Z);

    std::vector<ChunkPos> order;
    order.reserve(m_chunks.size());

    std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>, ChunkPosHash>::iterator
            chunkIt;
    for (chunkIt = m_chunks.begin(); chunkIt != m_chunks.end(); ++chunkIt)
    {
        order.push_back(chunkIt->first);
    }

    std::sort(order.begin(), order.end(), [&](const ChunkPos &a, const ChunkPos &b) {
        int adx = a.x - pcx;
        int adz = a.z - pcz;
        int bdx = b.x - pcx;
        int bdz = b.z - pcz;
        int da  = adx * adx + adz * adz;
        int db  = bdx * bdx + bdz * bdz;
        if (da != db)
        {
            return da < db;
        }
        if (a.x != b.x)
        {
            return a.x < b.x;
        }
        return a.z < b.z;
    });

    for (const ChunkPos &pos : order)
    {
        if (m_skyQueued.insert(pos).second)
        {
            m_skyQueue.push_back(pos);
        }
    }
}

void LevelRenderer::pumpSkyLightClampUpdate(int scheduleBudget, int applyBudget)
{
    uint8_t clamp = m_skyClampTarget;

    while (scheduleBudget-- > 0)
    {
        if (m_skyQueue.empty())
        {
            break;
        }

        ChunkPos pos = m_skyQueue.front();
        m_skyQueue.pop_front();

        std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>,
                           ChunkPosHash>::iterator it = m_chunks.find(pos);
        if (it == m_chunks.end())
        {
            continue;
        }

        std::vector<std::unique_ptr<ChunkMesh>> &meshes = it->second;

        for (size_t i = 0; i < meshes.size(); i++)
        {
            ChunkMesh *mesh = meshes[i].get();
            if (!mesh)
            {
                continue;
            }

            uint64_t id = mesh->getId();

            std::vector<uint16_t> rawLights;
            std::vector<float> shades;
            std::vector<uint32_t> tints;
            mesh->snapshotSky(&rawLights, &shades, &tints);

            if (rawLights.empty() || shades.size() != rawLights.size() ||
                tints.size() != rawLights.size())
            {
                continue;
            }

            m_mesherPool->detachTask([this, pos, i, id, clamp, rawLights = std::move(rawLights),
                                      shades = std::move(shades),
                                      tints  = std::move(tints)]() mutable {
                ThreadStorage::useDefaultThreadStorage();
                size_t vc = rawLights.size();
                std::vector<uint8_t> lightData;
                lightData.resize(vc * 3);
                for (size_t v = 0; v < vc; v++)
                {
                    uint16_t raw     = rawLights[v];
                    float shade      = shades[v];
                    uint32_t tint    = tints[v];
                    uint8_t tintMode = decodeTintMode(tint);
                    uint8_t br       = (uint8_t) (raw & 15);
                    uint8_t bg       = (uint8_t) ((raw >> 4) & 15);
                    uint8_t bb       = (uint8_t) ((raw >> 8) & 15);
                    uint8_t sky      = (uint8_t) ((raw >> 12) & 15);
                    if (sky > clamp)
                    {
                        sky = clamp;
                    }

                    uint8_t lr     = br > sky ? br : sky;
                    uint8_t lg     = bg > sky ? bg : sky;
                    uint8_t lb     = bb > sky ? bb : sky;
                    float minLight = 0.03f;
                    float baseR    = (float) lr / 15.0f;
                    float baseG    = (float) lg / 15.0f;
                    float baseB    = (float) lb / 15.0f;
                    if (baseR < minLight)
                    {
                        baseR = minLight;
                    }
                    if (baseG < minLight)
                    {
                        baseG = minLight;
                    }
                    if (baseB < minLight)
                    {
                        baseB = minLight;
                    }

                    float tr = 1.0f;
                    float tg = 1.0f;
                    float tb = 1.0f;
                    if (tintMode != 255)
                    {
                        tr = (float) ((tint >> 16) & 0xFF) / 255.0f;
                        tg = (float) ((tint >> 8) & 0xFF) / 255.0f;
                        tb = (float) (tint & 0xFF) / 255.0f;
                    }
                    float r             = baseR * shade * tr;
                    float g             = baseG * shade * tg;
                    float b             = baseB * shade * tb;
                    size_t base         = v * 3;
                    lightData[base + 0] = (uint8_t) (Mth::clampf(r, 0.0f, 1.0f) * 255.0f + 0.5f);
                    lightData[base + 1] = (uint8_t) (Mth::clampf(g, 0.0f, 1.0f) * 255.0f + 0.5f);
                    lightData[base + 2] = (uint8_t) (Mth::clampf(b, 0.0f, 1.0f) * 255.0f + 0.5f);
                }

                SkyUpdateResult result;
                result.pos       = pos;
                result.index     = i;
                result.meshId    = id;
                result.lightData = std::move(lightData);

                std::lock_guard<std::mutex> lock(m_skyMutex);

                m_skyResults.push_back(std::move(result));
            });
        }
    }

    while (applyBudget-- > 0)
    {
        SkyUpdateResult result;
        {
            std::lock_guard<std::mutex> lock(m_skyMutex);

            if (m_skyResults.empty())
            {
                break;
            }

            result = std::move(m_skyResults.front());
            m_skyResults.pop_front();
        }

        std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>,
                           ChunkPosHash>::iterator it = m_chunks.find(result.pos);
        if (it == m_chunks.end())
        {
            continue;
        }

        std::vector<std::unique_ptr<ChunkMesh>> &meshes = it->second;
        if (result.index >= meshes.size())
        {
            continue;
        }

        ChunkMesh *mesh = meshes[result.index].get();
        if (!mesh || mesh->getId() != result.meshId)
        {
            continue;
        }
        mesh->applySky(std::move(result.lightData));
    }
}
std::vector<uint8_t> LevelRenderer::buildCloudMap()
{
    std::vector<uint8_t> cloudMap = MemoryTracker::getInstance()->createByteBuffer(256 * 256);

    for (int z = 0; z < 256; z++)
    {
        for (int x = 0; x < 256; x++)
        {
            uint32_t h = (uint32_t) (x * 3129871) ^ (uint32_t) (z * 116129781);
            h          = h * h * 42317861u + h * 11u;
            cloudMap[(size_t) x + (size_t) 256 * (size_t) z] = ((h >> 16) & 15) < 8 ? 1 : 0;
        }
    }

    std::vector<uint8_t> temp = MemoryTracker::getInstance()->createByteBuffer(256 * 256);

    for (int pass = 0; pass < 3; pass++)
    {
        for (int z = 0; z < 256; z++)
        {
            for (int x = 0; x < 256; x++)
            {
                int count = 0;
                for (int dz = -1; dz <= 1; dz++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int nx = (x + dx) & 255;
                        int nz = (z + dz) & 255;
                        count += cloudMap[(size_t) nx + (size_t) 256 * (size_t) nz] ? 1 : 0;
                    }
                }

                temp[(size_t) x + (size_t) 256 * (size_t) z] = count >= 5 ? 1 : 0;
            }
        }

        cloudMap.swap(temp);
    }

    return cloudMap;
}

const std::vector<uint8_t> &LevelRenderer::getCloudMap()
{
    static std::vector<uint8_t> cloudMap = buildCloudMap();
    return cloudMap;
}

std::vector<float> LevelRenderer::buildStarVertices()
{
    std::vector<float> vertices = MemoryTracker::getInstance()->createFloatBuffer(1500 * 6 * 3);
    vertices.clear();
    vertices.reserve(1500 * 6 * 3);

    Random random(0);

    const float starSphereRadius = 160.0f;

    for (int i = 0; i < 1500; i++)
    {
        float x = random.nextFloat() * 2.0f - 1.0f;
        float y = random.nextFloat() * 2.0f - 1.0f;
        float z = random.nextFloat() * 2.0f - 1.0f;

        float d2 = x * x + y * y + z * z;
        if (d2 <= 0.01f || d2 >= 1.0f)
        {
            continue;
        }

        float invertedLength = 1.0f / sqrtf(d2);
        Vec3 direction(x * invertedLength, y * invertedLength, z * invertedLength);

        float size = 0.15f + random.nextFloat() * 0.10f;

        Vec3 up = fabs(direction.y) > 0.9f ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);

        Vec3 tangent = direction.cross(up).normalize();
        Vec3 bitan   = direction.cross(tangent).normalize();

        Vec3 center = direction.scale(starSphereRadius);

        Vec3 q0 = center.add(tangent.scale(-size)).add(bitan.scale(-size));
        Vec3 q1 = center.add(tangent.scale(size)).add(bitan.scale(-size));
        Vec3 q2 = center.add(tangent.scale(size)).add(bitan.scale(size));
        Vec3 q3 = center.add(tangent.scale(-size)).add(bitan.scale(size));

        vertices.push_back((float) q0.x);
        vertices.push_back((float) q0.y);
        vertices.push_back((float) q0.z);
        vertices.push_back((float) q1.x);
        vertices.push_back((float) q1.y);
        vertices.push_back((float) q1.z);
        vertices.push_back((float) q2.x);
        vertices.push_back((float) q2.y);
        vertices.push_back((float) q2.z);

        vertices.push_back((float) q0.x);
        vertices.push_back((float) q0.y);
        vertices.push_back((float) q0.z);
        vertices.push_back((float) q2.x);
        vertices.push_back((float) q2.y);
        vertices.push_back((float) q2.z);
        vertices.push_back((float) q3.x);
        vertices.push_back((float) q3.y);
        vertices.push_back((float) q3.z);
    }

    return vertices;
}

void LevelRenderer::setupMatrices(const Vec3 &cameraPos)
{
    GlStateManager::pushMatrix();
    GlStateManager::loadIdentity();
    GlStateManager::translatef(-cameraPos.x, -cameraPos.y, -cameraPos.z);
}

void LevelRenderer::setupFog()
{
    Fog &fog = m_level->getFog();
    if (!fog.isEnabled())
    {
        GlStateManager::disableFog();
        return;
    }

    if (fog.isDisabledInCaves())
    {
        LocalPlayer *me = Minecraft::getInstance()->getLocalPlayer();
        if (me)
        {
            const Vec3 &pos = me->getPosition();
            BlockPos blockPos((int) floor(pos.x), (int) floor(pos.y), (int) floor(pos.z));
            if (m_level->getSkyLightLevel(blockPos) == 0)
            {
                GlStateManager::disableFog();
                return;
            }
        }
    }

    GlStateManager::enableFog();

    float fogR = (float) fog.getColor().x;
    float fogG = (float) fog.getColor().y;
    float fogB = (float) fog.getColor().z;

    Texture *fogColormap = ColormapManager::getInstance()->get("fog");
    if (fogColormap)
    {
        int px = (int) (fog.getLutX() * (float) (fogColormap->getPixelWidth() - 1));
        int py = (int) (fog.getLutY() * (float) (fogColormap->getPixelHeight() - 1));
        fogColormap->samplePixel(px, py, &fogR, &fogG, &fogB);
    }

    float br = m_level->getDaylightFactor();

    fogR *= br * 0.94f + 0.06f;
    fogG *= br * 0.94f + 0.06f;
    fogB *= br * 0.91f + 0.09f;

    GlStateManager::setFogColor(fogR, fogG, fogB);

    float renderEnd = (float) (m_level->getRenderDistance() * Chunk::SIZE_X);
    float fogStart  = renderEnd * fog.getStartFactor();
    float fogEnd    = renderEnd * fog.getEndFactor();
    if (fogEnd < fogStart + 0.001f)
    {
        fogEnd = fogStart + 0.001f;
    }
    GlStateManager::setFogRange(fogStart, fogEnd);
}

void LevelRenderer::renderSky(const Mat4 &viewMatrix, const Mat4 &projection)
{
    GlStateManager::disableDepthTest();

    m_skyShader->bind();

    m_skyShader->setInt("u_skyColormap", 0);
    ColormapManager::getInstance()->bind("sky", 0);

    m_skyShader->setInt("u_fogColormap", 1);
    ColormapManager::getInstance()->bind("fog", 1);

    Fog &fog = m_level->getFog();
    m_skyShader->setVec2("u_fogLut", fog.getLutX(), fog.getLutY());
    m_skyShader->setVec3("u_skyLut", fog.getLutX(), 0.30f, 0.70f);

    m_skyShader->setInt("u_fogEnabled", GlStateManager::isFogEnabled());

    const DimensionTime &dimensionTime = m_level->getDimensionTime();

    m_skyShader->setFloat("u_celestialAngle", dimensionTime.getCelestialAngle());
    m_skyShader->setFloat("u_dayFactor", dimensionTime.getDaylightFactor());

    Vec3 sunDir = dimensionTime.getSunDirection();
    m_skyShader->setVec3("u_sunDir", sunDir.x, sunDir.y, sunDir.z);

    Mat4 invertedProjection = projection.inverse();
    m_skyShader->setMat4("u_invProj", invertedProjection.data);

    Mat4 viewRotationOnly     = viewMatrix;
    viewRotationOnly.data[12] = 0.0f;
    viewRotationOnly.data[13] = 0.0f;
    viewRotationOnly.data[14] = 0.0f;

    m_skyShader->setMat4("u_invViewRot", viewRotationOnly.inverse().data);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);

    GlStateManager::enableDepthTest();
}

void LevelRenderer::renderFastClouds(const Mat4 &viewMatrix, const Mat4 &projection)
{
    Minecraft *minecraft                 = Minecraft::getInstance();
    const Vec3 cameraPos                 = minecraft->getCamera()->getPosition();
    const std::vector<uint8_t> &cloudMap = getCloudMap();

    static float cloudOffset = 0.0f;
    cloudOffset += (float) Time::getDelta() * 0.8f;
    if (cloudOffset > 3072.0f)
    {
        cloudOffset -= 3072.0f;
    }

    float renderEnd = (float) (m_level->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    const float cloudY = 128.0f;

    int camCellX    = (int) floor((cameraPos.x + cloudOffset) / (float) cellSize);
    int camCellZ    = (int) floor(cameraPos.z / (float) cellSize);
    int radiusCells = (int) ceil(radius / (float) cellSize);

    int gridWidth  = radiusCells * 2 + 1;
    int gridHeight = radiusCells * 2 + 1;

    std::vector<uint8_t> mask((size_t) gridWidth * (size_t) gridHeight);

    for (int z = 0; z < gridHeight; z++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            int wx = camCellX + (x - radiusCells);
            int wz = camCellZ + (z - radiusCells);
            mask[(size_t) x + (size_t) gridWidth * (size_t) z] =
                    cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
        }
    }

    float baseX = (float) (camCellX - radiusCells) * (float) cellSize - cloudOffset;
    float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

    std::vector<float> vertices;
    vertices.reserve(4096);

    std::vector<uint8_t> topMask = mask;

    for (int z = 0; z < gridHeight; z++)
    {
        for (int x = 0; x < gridWidth; x++)
        {
            if (!topMask[(size_t) x + (size_t) gridWidth * (size_t) z])
            {
                continue;
            }

            int rw = 1;
            while (x + rw < gridWidth &&
                   topMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z])
            {
                rw++;
            }

            int rh  = 1;
            bool ok = true;
            while (z + rh < gridHeight && ok)
            {
                for (int k = 0; k < rw; k++)
                {
                    if (!topMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)])
                    {
                        ok = false;
                        break;
                    }
                }

                if (ok)
                {
                    rh++;
                }
            }

            for (int zz = 0; zz < rh; zz++)
            {
                for (int xx = 0; xx < rw; xx++)
                {
                    topMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;
                }
            }

            float x1 = baseX + (float) x * (float) cellSize;
            float z1 = baseZ + (float) z * (float) cellSize;
            float x2 = baseX + (float) (x + rw) * (float) cellSize;
            float z2 = baseZ + (float) (z + rh) * (float) cellSize;

            pushCloudVertex(&vertices, x1, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushCloudVertex(&vertices, x2, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushCloudVertex(&vertices, x2, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
            pushCloudVertex(&vertices, x1, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushCloudVertex(&vertices, x2, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
            pushCloudVertex(&vertices, x1, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
        }
    }

    m_cloudMesh.upload(vertices);

    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GlStateManager::enablePolygonOffsetFill();
    GlStateManager::setPolygonOffset(1.0f, 1.0f);

    m_cloudShader->bind();
    m_cloudShader->setFloat("u_cloudLight", m_cloudLightSmooth);
    m_cloudShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_cloudShader->setMat4("u_view", viewMatrix.data);
    m_cloudShader->setMat4("u_projection", projection.data);

    m_cloudMesh.render();

    GlStateManager::disablePolygonOffsetFill();
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
}

void LevelRenderer::renderClouds(const Mat4 &viewMatrix, const Mat4 &projection)
{
    Minecraft *minecraft                 = Minecraft::getInstance();
    const Vec3 cameraPos                 = minecraft->getCamera()->getPosition();
    const std::vector<uint8_t> &cloudMap = getCloudMap();

    m_cloudOffset += (float) Time::getDelta() * 0.8f;
    if (m_cloudOffset > 3072.0f)
    {
        m_cloudOffset -= 3072.0f;
    }

    float renderEnd = (float) (m_level->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    const float thick  = 4.0f;
    const float cloudY = 128.0f;

    int camCellX = (int) floor((cameraPos.x + m_cloudOffset) / (float) cellSize);
    int camCellZ = (int) floor(cameraPos.z / (float) cellSize);

    bool cloudDirty = (camCellX != m_cloudLastCamCellX || camCellZ != m_cloudLastCamCellZ);

    int radiusCells = (int) ceil(radius / (float) cellSize);

    if (cloudDirty)
    {
        m_cloudLastCamCellX    = camCellX;
        m_cloudLastCamCellZ    = camCellZ;
        m_cloudLastBuiltOffset = m_cloudOffset;

        int gridWidth  = radiusCells * 2 + 1;
        int gridHeight = radiusCells * 2 + 1;

        std::vector<uint8_t> mask((size_t) gridWidth * (size_t) gridHeight);

        for (int z = 0; z < gridHeight; z++)
        {
            for (int x = 0; x < gridWidth; x++)
            {
                int wx = camCellX + (x - radiusCells);
                int wz = camCellZ + (z - radiusCells);
                mask[(size_t) x + (size_t) gridWidth * (size_t) z] =
                        cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
            }
        }

        float baseX = (float) (camCellX - radiusCells) * (float) cellSize - m_cloudOffset;
        float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

        std::vector<float> vertices;
        vertices.reserve(4096);

        {
            std::vector<uint8_t> topMask = mask;

            for (int z = 0; z < gridHeight; z++)
            {
                for (int x = 0; x < gridWidth; x++)
                {
                    if (!topMask[(size_t) x + (size_t) gridWidth * (size_t) z])
                    {
                        continue;
                    }

                    int rw = 1;
                    while (x + rw < gridWidth &&
                           topMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z])
                    {
                        rw++;
                    }

                    int rh  = 1;
                    bool ok = true;
                    while (z + rh < gridHeight && ok)
                    {
                        for (int k = 0; k < rw; k++)
                        {
                            if (!topMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)])
                            {
                                ok = false;
                                break;
                            }
                        }

                        if (ok)
                        {
                            rh++;
                        }
                    }

                    for (int zz = 0; zz < rh; zz++)
                    {
                        for (int xx = 0; xx < rw; xx++)
                        {
                            topMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;
                        }
                    }

                    float x1 = baseX + (float) x * (float) cellSize;
                    float z1 = baseZ + (float) z * (float) cellSize;
                    float x2 = baseX + (float) (x + rw) * (float) cellSize;
                    float z2 = baseZ + (float) (z + rh) * (float) cellSize;
                    float y  = cloudY + thick;

                    pushCloudQuad(&vertices, x1, y, z1, x1, y, z2, x2, y, z2, x2, y, z1, 1.0f, 1.0f,
                                  1.0f, 0.875f);
                }
            }
        }

        {
            std::vector<uint8_t> botMask = mask;

            for (int z = 0; z < gridHeight; z++)
            {
                for (int x = 0; x < gridWidth; x++)
                {
                    if (!botMask[(size_t) x + (size_t) gridWidth * (size_t) z])
                    {
                        continue;
                    }

                    int rw = 1;
                    while (x + rw < gridWidth &&
                           botMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z])
                    {
                        rw++;
                    }

                    int rh  = 1;
                    bool ok = true;
                    while (z + rh < gridHeight && ok)
                    {
                        for (int k = 0; k < rw; k++)
                        {
                            if (!botMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)])
                            {
                                ok = false;
                                break;
                            }
                        }

                        if (ok)
                        {
                            rh++;
                        }
                    }

                    for (int zz = 0; zz < rh; zz++)
                    {
                        for (int xx = 0; xx < rw; xx++)
                        {
                            botMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;
                        }
                    }

                    float x1 = baseX + (float) x * (float) cellSize;
                    float z1 = baseZ + (float) z * (float) cellSize;
                    float x2 = baseX + (float) (x + rw) * (float) cellSize;
                    float z2 = baseZ + (float) (z + rh) * (float) cellSize;
                    float y  = cloudY;

                    pushCloudQuad(&vertices, x1, y, z2, x1, y, z1, x2, y, z1, x2, y, z2, 0.941f,
                                  0.941f, 0.941f, 0.875f);
                }
            }
        }

        for (int z = 0; z < gridHeight; z++)
        {
            for (int x = 0; x < gridWidth; x++)
            {
                if (!mask[(size_t) x + (size_t) gridWidth * (size_t) z])
                {
                    continue;
                }

                bool hasNX =
                        (x > 0) && (mask[(size_t) (x - 1) + (size_t) gridWidth * (size_t) z] != 0);
                bool hasPX = (x < gridWidth - 1) &&
                             (mask[(size_t) (x + 1) + (size_t) gridWidth * (size_t) z] != 0);
                bool hasNZ =
                        (z > 0) && (mask[(size_t) x + (size_t) gridWidth * (size_t) (z - 1)] != 0);
                bool hasPZ = (z < gridHeight - 1) &&
                             (mask[(size_t) x + (size_t) gridWidth * (size_t) (z + 1)] != 0);

                float x1 = baseX + (float) x * (float) cellSize;
                float z1 = baseZ + (float) z * (float) cellSize;
                float x2 = x1 + (float) cellSize;
                float z2 = z1 + (float) cellSize;
                float y1 = cloudY;
                float y2 = cloudY + thick;

                if (!hasNX)
                {
                    pushCloudQuad(&vertices, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, 0.847f,
                                  0.847f, 0.847f, 0.875f);
                }
                if (!hasPX)
                {
                    pushCloudQuad(&vertices, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, 0.847f,
                                  0.847f, 0.847f, 0.875f);
                }
                if (!hasNZ)
                {
                    pushCloudQuad(&vertices, x2, y1, z1, x1, y1, z1, x1, y2, z1, x2, y2, z1, 0.847f,
                                  0.847f, 0.847f, 0.875f);
                }
                if (!hasPZ)
                {
                    pushCloudQuad(&vertices, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, 0.847f,
                                  0.847f, 0.847f, 0.875f);
                }
            }
        }

        m_cloudMesh.upload(vertices);
    }

    bool cameraInsideCloud = false;
    {
        const float y1 = cloudY;
        const float y2 = cloudY + thick;

        if (cameraPos.y >= y1 && cameraPos.y <= y2)
        {
            int cellX = (int) floor((cameraPos.x + m_cloudOffset) / (float) cellSize);
            int cellZ = (int) floor(cameraPos.z / (float) cellSize);

            cameraInsideCloud =
                    (cloudMap[(size_t) (cellX & 255) + (size_t) 256 * (size_t) (cellZ & 255)] != 0);
        }
    }

    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(cameraInsideCloud ? GL_FRONT : GL_BACK);

    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderCommand::enablePolygonOffsetFill();
    RenderCommand::setPolygonOffset(1.0f, 1.0f);

    m_cloudShader->bind();
    m_cloudShader->setFloat("u_cloudLight", m_cloudLightSmooth);

    float dx = m_cloudLastBuiltOffset - m_cloudOffset;

    GlStateManager::pushMatrix();
    GlStateManager::translatef(dx, 0.0f, 0.0f);

    m_cloudShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_cloudShader->setMat4("u_view", viewMatrix.data);
    m_cloudShader->setMat4("u_projection", projection.data);

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(true);
    GlStateManager::disableBlend();
    GlStateManager::setColorMask(false, false, false, false);

    m_cloudMesh.render();

    GlStateManager::setColorMask(true, true, true, true);
    GlStateManager::enableBlend();
    GlStateManager::setDepthMask(false);
    GlStateManager::setDepthFunc(GL_EQUAL);

    m_cloudMesh.render();

    GlStateManager::setDepthFunc(GL_LEQUAL);
    GlStateManager::setDepthMask(true);

    GlStateManager::popMatrix();

    RenderCommand::disablePolygonOffsetFill();
    GlStateManager::disableBlend();
    GlStateManager::setCullFace(GL_BACK);
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setColorMask(true, true, true, true);
    GlStateManager::setDepthFunc(GL_LEQUAL);
    GlStateManager::setDepthMask(true);
    GlStateManager::enableCull();
}

void LevelRenderer::renderStars(const Mat4 &viewMatrix, const Mat4 &projection)
{
    const DimensionTime &dimensionTime = m_level->getDimensionTime();

    float td    = dimensionTime.getCelestialAngle();
    float alpha = 1.0f - (cosf(td * 6.28318530718f) * 2.0f + 0.25f);
    if (alpha < 0.0f)
    {
        alpha = 0.0f;
    }
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }
    alpha = alpha * alpha * 0.5f;
    if (alpha < 0.01f)
    {
        return;
    }

    Mat4 viewRotOnly     = viewMatrix;
    viewRotOnly.data[12] = 0.0f;
    viewRotOnly.data[13] = 0.0f;
    viewRotOnly.data[14] = 0.0f;

    GlStateManager::disableDepthTest();

    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_starShader->bind();
    m_starShader->setMat4("u_viewRot", viewRotOnly.data);
    m_starShader->setMat4("u_projection", projection.data);

    const float starSpeed = 1.0f;
    m_starShader->setFloat("u_starAngle",
                           dimensionTime.getCelestialAngle() * 6.28318530718f * starSpeed);
    m_starShader->setFloat("u_starAlpha", alpha);

    RenderCommand::bindVertexArray(m_starVao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, m_starVertexCount);
    RenderCommand::bindVertexArray(0);

    GlStateManager::disableBlend();
    GlStateManager::enableDepthTest();
}
