#include "WorldRenderer.h"

#include <cmath>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../rendering/ColormapManager.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/ImmediateRenderer.h"
#include "../rendering/RenderCommand.h"
#include "../utils/Random.h"
#include "../utils/Time.h"
#include "../utils/math/Math.h"
#include "models/Model.h"
#include "models/ModelDefinition.h"
#include "models/ModelPartsSkinned.h"

WorldRenderer::WorldRenderer(World *world, int width, int height)
    : m_worldShader(nullptr), m_skyShader(nullptr), m_cloudShader(nullptr), m_fogShader(nullptr), m_starShader(nullptr), m_starVao(0), m_starVbo(0), m_starVertexCount(0), m_sceneFramebuffer(nullptr), m_world(world), m_renderChunkGrid(false),
      m_cloudOffset(0.0f), m_cloudLastCamCellX(INT_MIN), m_cloudLastCamCellZ(INT_MIN), m_cloudLastBuiltOffset(-99999.0f), m_cloudLightSmooth(1.0f), m_lastSkyLightClamp(255) {
    m_worldShader = new Shader("shaders/world.vert", "shaders/world.frag");
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
    RenderCommand::uploadArrayBuffer(stars.data(), (uint32_t) (stars.size() * sizeof(float)), GL_STATIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * (uint32_t) sizeof(float), 0);

    RenderCommand::bindArrayBuffer(0);
    RenderCommand::bindVertexArray(0);

    m_sceneFramebuffer = new Framebuffer(width, height);

    ColormapManager *colormapManager = ColormapManager::getInstance();
    colormapManager->load("fog", "textures/colormap/fog.png");
    colormapManager->load("sky", "textures/colormap/sky.png");
    colormapManager->load("grasscolor", "textures/colormap/grasscolor.png");

    m_mesherRunning = true;
    m_mesherPool    = std::make_unique<BS::thread_pool<>>((size_t) std::max(1u, std::thread::hardware_concurrency() - 2));
}

WorldRenderer::~WorldRenderer() {
    m_mesherRunning = false;
    if (m_mesherPool) {
        m_mesherPool->wait();
        m_mesherPool.reset();
    }

    if (m_sceneFramebuffer) {
        delete m_sceneFramebuffer;
        m_sceneFramebuffer = nullptr;
    }

    if (m_starVbo) RenderCommand::deleteBuffer(m_starVbo);
    if (m_starVao) RenderCommand::deleteVertexArray(m_starVao);

    if (m_starShader) {
        delete m_starShader;
        m_starShader = nullptr;
    }

    if (m_fogShader) {
        delete m_fogShader;
        m_fogShader = nullptr;
    }

    if (m_cloudShader) {
        delete m_cloudShader;
        m_cloudShader = nullptr;
    }

    if (m_skyShader) {
        delete m_skyShader;
        m_skyShader = nullptr;
    }

    if (m_worldShader) {
        delete m_worldShader;
        m_worldShader = nullptr;
    }
}

void WorldRenderer::onResize(int width, int height) { m_sceneFramebuffer->resize(width, height); }

void WorldRenderer::render(float partialTicks) {
    Minecraft *minecraft = Minecraft::getInstance();
    const Camera *camera = minecraft->getCamera();

    const Vec3 cameraPos = camera->getPosition();
    Mat4 viewMatrix      = camera->getViewMatrix();
    Mat4 projection      = minecraft->getProjection();

    const WorldTime &worldTime = m_world->getWorldTime();

    float blendFactor = 1.0f - expf(-Time::getDelta() * 2.5f);
    m_cloudLightSmooth += (worldTime.getDaylightFactor() - m_cloudLightSmooth) * blendFactor;

    setupMatrices(cameraPos);
    setupFog();

    ImmediateRenderer::getForWorld()->setViewProjection(viewMatrix, projection);
    minecraft->getDefaultFont()->setNearest(true);
    minecraft->getDefaultFont()->setWorldViewProjection(viewMatrix, projection);

    m_sceneFramebuffer->bind();
    RenderCommand::clearAll();

    renderSky(viewMatrix, projection);
    renderStars(viewMatrix, projection);

    bindWorldShader(cameraPos, viewMatrix, projection);

    {
        uint8_t clamp = worldTime.getSkyLightClamp();
        if (clamp != m_lastSkyLightClamp) {
            m_lastSkyLightClamp = clamp;

            for (auto &kv : m_chunks) {
                const ChunkPos pos = kv.first;
                auto &meshes       = kv.second;

                for (size_t i = 0; i < meshes.size(); i++) {
                    ChunkMesh *mesh = meshes[i].get();
                    if (!mesh) continue;

                    uint64_t id = mesh->getId();

                    std::vector<float> vertices;
                    std::vector<uint16_t> rawLights;
                    std::vector<float> shades;
                    std::vector<uint32_t> tints;
                    mesh->snapshotSky(vertices, rawLights, shades, tints);

                    if (vertices.empty()) continue;
                    if (rawLights.size() * 8 != vertices.size()) continue;
                    if (shades.size() != rawLights.size()) continue;
                    if (tints.size() != rawLights.size()) continue;

                    m_mesherPool->detach_task([this, pos, i, id, clamp, vertices = std::move(vertices), rawLights = std::move(rawLights), shades = std::move(shades), tints = std::move(tints)]() mutable {
                        size_t vc = rawLights.size();
                        for (size_t v = 0; v < vc; v++) {
                            uint16_t raw  = rawLights[v];
                            float shade   = shades[v];
                            uint32_t tint = tints[v];

                            uint8_t br  = (uint8_t) (raw & 15);
                            uint8_t bg  = (uint8_t) ((raw >> 4) & 15);
                            uint8_t bb  = (uint8_t) ((raw >> 8) & 15);
                            uint8_t sky = (uint8_t) ((raw >> 12) & 15);

                            if (sky > clamp) sky = clamp;

                            uint8_t lr = br > sky ? br : sky;
                            uint8_t lg = bg > sky ? bg : sky;
                            uint8_t lb = bb > sky ? bb : sky;

                            float minLight = 0.03f;

                            float baseR = (float) lr / 15.0f;
                            float baseG = (float) lg / 15.0f;
                            float baseB = (float) lb / 15.0f;

                            if (baseR < minLight) baseR = minLight;
                            if (baseG < minLight) baseG = minLight;
                            if (baseB < minLight) baseB = minLight;

                            float tr = (float) ((tint >> 16) & 0xFF) / 255.0f;
                            float tg = (float) ((tint >> 8) & 0xFF) / 255.0f;
                            float tb = (float) (tint & 0xFF) / 255.0f;

                            float r = baseR * shade * tr;
                            float g = baseG * shade * tg;
                            float b = baseB * shade * tb;

                            size_t base        = v * 8 + 5;
                            vertices[base + 0] = r;
                            vertices[base + 1] = g;
                            vertices[base + 2] = b;
                        }

                        SkyUpdateResult res;
                        res.pos      = pos;
                        res.index    = i;
                        res.meshId   = id;
                        res.vertices = std::move(vertices);

                        std::lock_guard<std::mutex> lock(m_skyMutex);
                        m_skyResults.push_back(std::move(res));
                    });
                }
            }
        }

        int budget = 32;
        while (budget-- > 0) {
            SkyUpdateResult res;
            {
                std::lock_guard<std::mutex> lock(m_skyMutex);
                if (m_skyResults.empty()) break;
                res = std::move(m_skyResults.front());
                m_skyResults.pop_front();
            }

            auto it = m_chunks.find(res.pos);
            if (it == m_chunks.end()) continue;
            auto &meshes = it->second;
            if (res.index >= meshes.size()) continue;

            ChunkMesh *mesh = meshes[res.index].get();
            if (!mesh) continue;
            if (mesh->getId() != res.meshId) continue;

            mesh->applySky(std::move(res.vertices));
        }
    }

    uploadPendingMeshes();
    renderChunks(viewMatrix, projection);

    renderBlockOutline();

    renderClouds(viewMatrix, projection);

    renderEntities(partialTicks);
    renderEntityNameTags(partialTicks);

    m_sceneFramebuffer->unbind();

    renderFogPass(projection);

    if (m_renderChunkGrid) renderChunkGrid();

    GlStateManager::popMatrix();
}

void WorldRenderer::rebuild() {
    m_chunks.clear();

    for (const auto &[pos, chunkPtr] : m_world->getChunks()) {
        std::vector<ChunkMesher::MeshBuildResult> results;
        ChunkMesher::buildMeshes(m_world, chunkPtr.get(), results);

        std::vector<std::unique_ptr<ChunkMesh>> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results) {
            std::unique_ptr<ChunkMesh> renderMesh;
            renderMesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size(), std::move(result.rawLights), std::move(result.shades), std::move(result.tints));
            meshes.push_back(std::move(renderMesh));
        }

        m_chunks.emplace(pos, std::move(meshes));
    }
}

void WorldRenderer::rebuildChunk(const ChunkPos &pos) {
    if (m_world->getChunks().find(pos) == m_world->getChunks().end()) return;

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);

        if (m_rebuildQueued.insert(pos).second) m_rebuildQueue.push(pos);
    }

    scheduleMesher();
}

void WorldRenderer::rebuildChunkUrgent(const ChunkPos &pos) {
    if (m_world->getChunks().find(pos) == m_world->getChunks().end()) return;

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);

        if (m_urgentQueued.insert(pos).second) m_urgentQueue.push(pos);
    }

    scheduleMesher();
}

std::vector<uint8_t> WorldRenderer::buildCloudMap() {
    std::vector<uint8_t> cloudMap(256 * 256);

    for (int z = 0; z < 256; z++)
        for (int x = 0; x < 256; x++) {
            uint32_t h                                       = (uint32_t) (x * 3129871) ^ (uint32_t) (z * 116129781);
            h                                                = h * h * 42317861u + h * 11u;
            cloudMap[(size_t) x + (size_t) 256 * (size_t) z] = ((h >> 16) & 15) < 8 ? 1 : 0;
        }

    std::vector<uint8_t> temp(256 * 256);

    for (int pass = 0; pass < 3; pass++) {
        for (int z = 0; z < 256; z++)
            for (int x = 0; x < 256; x++) {
                int count = 0;
                for (int dz = -1; dz <= 1; dz++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = (x + dx) & 255;
                        int nz = (z + dz) & 255;
                        count += cloudMap[(size_t) nx + (size_t) 256 * (size_t) nz] ? 1 : 0;
                    }

                temp[(size_t) x + (size_t) 256 * (size_t) z] = count >= 5 ? 1 : 0;
            }

        cloudMap.swap(temp);
    }

    return cloudMap;
}

const std::vector<uint8_t> &WorldRenderer::getCloudMap() {
    static std::vector<uint8_t> cloudMap = buildCloudMap();
    return cloudMap;
}

std::vector<float> WorldRenderer::buildStarVertices() {
    std::vector<float> vertices;
    vertices.reserve(1500 * 6 * 3);

    Random random(0);

    const float starSphereRadius = 100.0f;

    for (int i = 0; i < 1500; i++) {
        float x = random.nextFloat() * 2.0f - 1.0f;
        float y = random.nextFloat() * 2.0f - 1.0f;
        float z = random.nextFloat() * 2.0f - 1.0f;

        float d2 = x * x + y * y + z * z;
        if (d2 <= 0.01f) continue;
        if (d2 >= 1.0f) continue;

        float invertedLength = 1.0f / sqrtf(d2);
        Vec3 direction(x * invertedLength, y * invertedLength, z * invertedLength);

        float size = 0.25f + random.nextFloat() * 0.25f;

        Vec3 up;
        if (fabs(direction.y) > 0.9f) up = Vec3(1.0f, 0.0f, 0.0f);
        else
            up = Vec3(0.0f, 1.0f, 0.0f);

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

void WorldRenderer::submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results) {
    std::lock_guard<std::mutex> lock(m_meshQueueMutex);
    m_pendingMeshes.emplace_back(pos, std::move(results));
}

void WorldRenderer::setupMatrices(const Vec3 &cameraPos) {
    GlStateManager::pushMatrix();
    GlStateManager::loadIdentity();
    GlStateManager::translatef(-cameraPos.x, -cameraPos.y, -cameraPos.z);
}

void WorldRenderer::setupFog() {
    Fog &fog = m_world->getFog();
    if (!fog.isEnabled()) {
        GlStateManager::disableFog();
        return;
    }

    if (fog.isDisabledInCaves()) {
        LocalPlayer *me = Minecraft::getInstance()->getLocalPlayer();
        if (me) {
            const Vec3 &pos = me->getPosition();
            BlockPos blockPos((int) floor(pos.x), (int) floor(pos.y), (int) floor(pos.z));
            if (m_world->getSkyLightLevel(blockPos) == 0) {
                GlStateManager::disableFog();
                return;
            }
        }
    }

    GlStateManager::enableFog();

    const Vec3 &fogColor = fog.getColor();
    GlStateManager::setFogColor((float) fogColor.x, (float) fogColor.y, (float) fogColor.z);

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);

    float nightFactor = 1.0f - m_world->getDaylightFactor();

    float fogStartMul = 1.0f + nightFactor * 1.35f;
    float fogEndMul   = 1.0f + nightFactor * 2.25f;

    float fogStart = renderEnd * fog.getStartFactor() * fogStartMul;
    float fogEnd   = renderEnd * fog.getEndFactor() * fogEndMul;

    if (fogEnd < fogStart + 0.001f) fogEnd = fogStart + 0.001f;
    GlStateManager::setFogRange(fogStart, fogEnd);
}

void WorldRenderer::renderSky(const Mat4 &viewMatrix, const Mat4 &projection) {
    GlStateManager::disableDepthTest();

    m_skyShader->bind();

    m_skyShader->setInt("u_skyColormap", 0);
    ColormapManager::getInstance()->bind("sky", 0);

    m_skyShader->setInt("u_fogColormap", 1);
    ColormapManager::getInstance()->bind("fog", 1);

    Fog &fog = m_world->getFog();
    m_skyShader->setVec2("u_fogLut", fog.getLutX(), fog.getLutY());
    m_skyShader->setVec3("u_skyLut", 0.85f, 0.30f, 0.70f);

    m_skyShader->setInt("u_fogEnabled", GlStateManager::isFogEnabled());

    const WorldTime &worldTime = m_world->getWorldTime();

    m_skyShader->setFloat("u_celestialAngle", worldTime.getCelestialAngle());
    m_skyShader->setFloat("u_dayFactor", worldTime.getDaylightFactor());

    Vec3 sunDir = worldTime.getSunDirection();
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

void WorldRenderer::renderFastClouds(const Mat4 &viewMatrix, const Mat4 &projection) {
    Minecraft *minecraft                 = Minecraft::getInstance();
    const Vec3 cameraPos                 = minecraft->getCamera()->getPosition();
    const std::vector<uint8_t> &cloudMap = getCloudMap();

    static float cloudOffset = 0.0f;
    cloudOffset += (float) Time::getDelta() * 0.8f;
    if (cloudOffset > 3072.0f) cloudOffset -= 3072.0f;

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
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
        for (int x = 0; x < gridWidth; x++) {
            int wx                                             = camCellX + (x - radiusCells);
            int wz                                             = camCellZ + (z - radiusCells);
            mask[(size_t) x + (size_t) gridWidth * (size_t) z] = cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
        }

    float baseX = (float) (camCellX - radiusCells) * (float) cellSize - cloudOffset;
    float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

    std::vector<float> vertices;
    vertices.reserve(4096);

    auto pushVertex = [&](float x, float y, float z, float r, float g, float b, float a) {
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
        vertices.push_back(r);
        vertices.push_back(g);
        vertices.push_back(b);
        vertices.push_back(a);
    };

    std::vector<uint8_t> topMask = mask;

    for (int z = 0; z < gridHeight; z++)
        for (int x = 0; x < gridWidth; x++) {
            if (!topMask[(size_t) x + (size_t) gridWidth * (size_t) z]) continue;

            int rw = 1;
            while (x + rw < gridWidth && topMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z]) rw++;

            int rh  = 1;
            bool ok = true;
            while (z + rh < gridHeight && ok) {
                for (int k = 0; k < rw; k++)
                    if (!topMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)]) {
                        ok = false;
                        break;
                    }
                if (ok) rh++;
            }

            for (int zz = 0; zz < rh; zz++)
                for (int xx = 0; xx < rw; xx++) topMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;

            float x1 = baseX + (float) x * (float) cellSize;
            float z1 = baseZ + (float) z * (float) cellSize;
            float x2 = baseX + (float) (x + rw) * (float) cellSize;
            float z2 = baseZ + (float) (z + rh) * (float) cellSize;

            pushVertex(x1, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushVertex(x2, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushVertex(x2, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
            pushVertex(x1, cloudY, z1, 1.0f, 1.0f, 1.0f, 0.875f);
            pushVertex(x2, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
            pushVertex(x1, cloudY, z2, 1.0f, 1.0f, 1.0f, 0.875f);
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

void WorldRenderer::renderClouds(const Mat4 &viewMatrix, const Mat4 &projection) {
    Minecraft *minecraft                 = Minecraft::getInstance();
    const Vec3 cameraPos                 = minecraft->getCamera()->getPosition();
    const std::vector<uint8_t> &cloudMap = getCloudMap();

    m_cloudOffset += (float) Time::getDelta() * 0.8f;
    if (m_cloudOffset > 3072.0f) m_cloudOffset -= 3072.0f;

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    const float thick  = 4.0f;
    const float cloudY = 128.0f;

    int camCellX = (int) floor((cameraPos.x + m_cloudOffset) / (float) cellSize);
    int camCellZ = (int) floor(cameraPos.z / (float) cellSize);

    bool cloudDirty = (camCellX != m_cloudLastCamCellX || camCellZ != m_cloudLastCamCellZ);

    int radiusCells = (int) ceil(radius / (float) cellSize);

    if (cloudDirty) {
        m_cloudLastCamCellX    = camCellX;
        m_cloudLastCamCellZ    = camCellZ;
        m_cloudLastBuiltOffset = m_cloudOffset;

        int gridWidth  = radiusCells * 2 + 1;
        int gridHeight = radiusCells * 2 + 1;

        std::vector<uint8_t> mask((size_t) gridWidth * (size_t) gridHeight);

        for (int z = 0; z < gridHeight; z++)
            for (int x = 0; x < gridWidth; x++) {
                int wx                                             = camCellX + (x - radiusCells);
                int wz                                             = camCellZ + (z - radiusCells);
                mask[(size_t) x + (size_t) gridWidth * (size_t) z] = cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
            }

        float baseX = (float) (camCellX - radiusCells) * (float) cellSize - m_cloudOffset;
        float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

        std::vector<float> vertices;
        vertices.reserve(4096);

        auto pushVertex = [&](float x, float y, float z, float r, float g, float b, float a) {
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(r);
            vertices.push_back(g);
            vertices.push_back(b);
            vertices.push_back(a);
        };

        auto pushQuad = [&](float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, float r, float g, float b, float a) {
            pushVertex(x1, y1, z1, r, g, b, a);
            pushVertex(x2, y2, z2, r, g, b, a);
            pushVertex(x3, y3, z3, r, g, b, a);
            pushVertex(x1, y1, z1, r, g, b, a);
            pushVertex(x3, y3, z3, r, g, b, a);
            pushVertex(x4, y4, z4, r, g, b, a);
        };

        {
            std::vector<uint8_t> topMask = mask;

            for (int z = 0; z < gridHeight; z++)
                for (int x = 0; x < gridWidth; x++) {
                    if (!topMask[(size_t) x + (size_t) gridWidth * (size_t) z]) continue;

                    int rw = 1;
                    while (x + rw < gridWidth && topMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z]) rw++;

                    int rh  = 1;
                    bool ok = true;
                    while (z + rh < gridHeight && ok) {
                        for (int k = 0; k < rw; k++)
                            if (!topMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)]) {
                                ok = false;
                                break;
                            }
                        if (ok) rh++;
                    }

                    for (int zz = 0; zz < rh; zz++)
                        for (int xx = 0; xx < rw; xx++) topMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;

                    float x1 = baseX + (float) x * (float) cellSize;
                    float z1 = baseZ + (float) z * (float) cellSize;
                    float x2 = baseX + (float) (x + rw) * (float) cellSize;
                    float z2 = baseZ + (float) (z + rh) * (float) cellSize;
                    float y  = cloudY + thick;

                    pushQuad(x1, y, z1, x1, y, z2, x2, y, z2, x2, y, z1, 1.0f, 1.0f, 1.0f, 0.875f);
                }
        }

        {
            std::vector<uint8_t> botMask = mask;

            for (int z = 0; z < gridHeight; z++)
                for (int x = 0; x < gridWidth; x++) {
                    if (!botMask[(size_t) x + (size_t) gridWidth * (size_t) z]) continue;

                    int rw = 1;
                    while (x + rw < gridWidth && botMask[(size_t) (x + rw) + (size_t) gridWidth * (size_t) z]) rw++;

                    int rh  = 1;
                    bool ok = true;
                    while (z + rh < gridHeight && ok) {
                        for (int k = 0; k < rw; k++)
                            if (!botMask[(size_t) (x + k) + (size_t) gridWidth * (size_t) (z + rh)]) {
                                ok = false;
                                break;
                            }
                        if (ok) rh++;
                    }

                    for (int zz = 0; zz < rh; zz++)
                        for (int xx = 0; xx < rw; xx++) botMask[(size_t) (x + xx) + (size_t) gridWidth * (size_t) (z + zz)] = 0;

                    float x1 = baseX + (float) x * (float) cellSize;
                    float z1 = baseZ + (float) z * (float) cellSize;
                    float x2 = baseX + (float) (x + rw) * (float) cellSize;
                    float z2 = baseZ + (float) (z + rh) * (float) cellSize;
                    float y  = cloudY;

                    pushQuad(x1, y, z2, x1, y, z1, x2, y, z1, x2, y, z2, 0.941f, 0.941f, 0.941f, 0.875f);
                }
        }

        for (int z = 0; z < gridHeight; z++)
            for (int x = 0; x < gridWidth; x++) {
                if (!mask[(size_t) x + (size_t) gridWidth * (size_t) z]) continue;

                bool hasNX = (x > 0) && (mask[(size_t) (x - 1) + (size_t) gridWidth * (size_t) z] != 0);
                bool hasPX = (x < gridWidth - 1) && (mask[(size_t) (x + 1) + (size_t) gridWidth * (size_t) z] != 0);
                bool hasNZ = (z > 0) && (mask[(size_t) x + (size_t) gridWidth * (size_t) (z - 1)] != 0);
                bool hasPZ = (z < gridHeight - 1) && (mask[(size_t) x + (size_t) gridWidth * (size_t) (z + 1)] != 0);

                float x1 = baseX + (float) x * (float) cellSize;
                float z1 = baseZ + (float) z * (float) cellSize;
                float x2 = x1 + (float) cellSize;
                float z2 = z1 + (float) cellSize;
                float y1 = cloudY;
                float y2 = cloudY + thick;

                if (!hasNX) pushQuad(x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, 0.847f, 0.847f, 0.847f, 0.875f);
                if (!hasPX) pushQuad(x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, 0.847f, 0.847f, 0.847f, 0.875f);
                if (!hasNZ) pushQuad(x2, y1, z1, x1, y1, z1, x1, y2, z1, x2, y2, z1, 0.847f, 0.847f, 0.847f, 0.875f);
                if (!hasPZ) pushQuad(x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, 0.847f, 0.847f, 0.847f, 0.875f);
            }

        m_cloudMesh.upload(vertices);
    }

    bool cameraInsideCloud = false;
    {
        const float y1 = cloudY;
        const float y2 = cloudY + thick;

        if (cameraPos.y >= y1 && cameraPos.y <= y2) {
            int cellX = (int) floor((cameraPos.x + m_cloudOffset) / (float) cellSize);
            int cellZ = (int) floor(cameraPos.z / (float) cellSize);

            cameraInsideCloud = (cloudMap[(size_t) (cellX & 255) + (size_t) 256 * (size_t) (cellZ & 255)] != 0);
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
    GlStateManager::enableCull();
}

void WorldRenderer::renderStars(const Mat4 &viewMatrix, const Mat4 &projection) {
    const WorldTime &worldTime = m_world->getWorldTime();

    float alpha = 1.0f - worldTime.getDaylightFactor();
    alpha       = alpha * alpha;

    if (alpha <= 0.001f) return;

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

    const float starSpeed = 0.05f;
    m_starShader->setFloat("u_starAngle", worldTime.getCelestialAngle() * 6.28318530718f * starSpeed);
    m_starShader->setFloat("u_starAlpha", alpha);

    RenderCommand::bindVertexArray(m_starVao);
    RenderCommand::renderArrays(GL_TRIANGLES, 0, m_starVertexCount);
    RenderCommand::bindVertexArray(0);

    GlStateManager::disableBlend();
    GlStateManager::enableDepthTest();
}

void WorldRenderer::bindWorldShader(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection) {
    m_worldShader->bind();
    m_worldShader->setInt("u_texture", 0);

    m_worldShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_worldShader->setMat4("u_view", viewMatrix.data);
    m_worldShader->setMat4("u_projection", projection.data);
    m_worldShader->setVec3("u_cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    GlStateManager::enableDepthTest();
}

void WorldRenderer::uploadPendingMeshes() {
    int uploadsPerFrame = 16;
    while (uploadsPerFrame-- > 0) {
        std::pair<ChunkPos, std::vector<ChunkMesher::MeshBuildResult>> pair;

        {
            std::lock_guard<std::mutex> lock(m_meshQueueMutex);

            if (m_pendingMeshes.empty()) break;

            pair = std::move(m_pendingMeshes.front());
            m_pendingMeshes.pop_front();
        }

        const ChunkPos &pos                                = pair.first;
        std::vector<ChunkMesher::MeshBuildResult> &results = pair.second;

        std::vector<std::unique_ptr<ChunkMesh>> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results) {
            std::unique_ptr<ChunkMesh> renderMesh;
            renderMesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size(), std::move(result.rawLights), std::move(result.shades), std::move(result.tints));
            meshes.push_back(std::move(renderMesh));
        }

        m_chunks[pos] = std::move(meshes);
    }
}

void WorldRenderer::renderChunks(const Mat4 &viewMatrix, const Mat4 &projection) {
    Minecraft *minecraft = Minecraft::getInstance();

    Vec3 playerPos     = minecraft->getLocalPlayer()->getPosition();
    int playerChunkX   = Math::floorDiv((int) playerPos.x, Chunk::SIZE_X);
    int playerChunkZ   = Math::floorDiv((int) playerPos.z, Chunk::SIZE_Z);
    int renderDistance = m_world->getRenderDistance();

    Mat4 viewProjection = projection.multiply(viewMatrix).multiply(GlStateManager::getMatrix());
    m_frustum.extractPlanes(viewProjection);

    for (const auto &[pos, meshes] : m_chunks) {
        int dx = pos.x - playerChunkX;
        int dz = pos.z - playerChunkZ;

        if (dx * dx + dz * dz > renderDistance * renderDistance) continue;

        Vec3 chunkMin(pos.x * Chunk::SIZE_X, pos.y * Chunk::SIZE_Y, pos.z * Chunk::SIZE_Z);
        Vec3 chunkMax(chunkMin.x + Chunk::SIZE_X, chunkMin.y + Chunk::SIZE_Y, chunkMin.z + Chunk::SIZE_Z);
        AABB chunkAABB(chunkMin, chunkMax);

        if (!m_frustum.testAABB(chunkAABB)) continue;

        for (const std::unique_ptr<ChunkMesh> &mesh : meshes) mesh->render();
    }
}

void WorldRenderer::renderChunkGrid() const {
    GlStateManager::disableDepthTest();

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->begin(GL_LINES);
    renderer->color(0xFF00FF00);

    for (const auto &[pos, chunkPtr] : m_world->getChunks()) {
        float minX = (float) (pos.x * Chunk::SIZE_X);
        float minY = (float) (pos.y * Chunk::SIZE_Y);
        float minZ = (float) (pos.z * Chunk::SIZE_Z);

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

void WorldRenderer::renderBlockOutline() {
    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 pos       = minecraft->getCamera()->getPosition();
    const Vec3 direction = minecraft->getLocalPlayer()->getFront().normalize();

    HitResult *hit = m_world->clip(pos, direction, 6.0f);
    if (!hit) return;

    if (!hit->isBlock()) {
        delete hit;
        return;
    }

    const BlockPos blockPos = hit->getBlockPos();
    delete hit;

    constexpr float EPS = 0.0035f;

    float minX = (float) blockPos.x - EPS;
    float minY = (float) blockPos.y - EPS;
    float minZ = (float) blockPos.z - EPS;
    float maxX = (float) blockPos.x + 1.0f + EPS;
    float maxY = (float) blockPos.y + 1.0f + EPS;
    float maxZ = (float) blockPos.z + 1.0f + EPS;

    GlStateManager::enableDepthTest();
    GlStateManager::setDepthMask(false);
    GlStateManager::disableCull();

    RenderCommand::setLineWidth(2.0f);

    ImmediateRenderer *immediateRenderer = ImmediateRenderer::getForWorld();
    immediateRenderer->begin(GL_LINES);
    immediateRenderer->color(0xFF000000);

    immediateRenderer->vertex(minX, minY, minZ);
    immediateRenderer->vertex(maxX, minY, minZ);
    immediateRenderer->vertex(maxX, minY, minZ);
    immediateRenderer->vertex(maxX, minY, maxZ);
    immediateRenderer->vertex(maxX, minY, maxZ);
    immediateRenderer->vertex(minX, minY, maxZ);
    immediateRenderer->vertex(minX, minY, maxZ);
    immediateRenderer->vertex(minX, minY, minZ);

    immediateRenderer->vertex(minX, maxY, minZ);
    immediateRenderer->vertex(maxX, maxY, minZ);
    immediateRenderer->vertex(maxX, maxY, minZ);
    immediateRenderer->vertex(maxX, maxY, maxZ);
    immediateRenderer->vertex(maxX, maxY, maxZ);
    immediateRenderer->vertex(minX, maxY, maxZ);
    immediateRenderer->vertex(minX, maxY, maxZ);
    immediateRenderer->vertex(minX, maxY, minZ);

    immediateRenderer->vertex(minX, minY, minZ);
    immediateRenderer->vertex(minX, maxY, minZ);
    immediateRenderer->vertex(maxX, minY, minZ);
    immediateRenderer->vertex(maxX, maxY, minZ);
    immediateRenderer->vertex(maxX, minY, maxZ);
    immediateRenderer->vertex(maxX, maxY, maxZ);
    immediateRenderer->vertex(minX, minY, maxZ);
    immediateRenderer->vertex(minX, maxY, maxZ);

    immediateRenderer->end();

    GlStateManager::setDepthMask(true);
    GlStateManager::enableCull();
}

void WorldRenderer::renderEntities(float partialTicks) {
    for (const std::unique_ptr<Entity> &entity : m_world->getEntities()) {
        EntityRenderer *renderer = EntityRendererRegistry::get()->getValue(entity->getType());
        renderer->render(entity.get(), partialTicks);
    }
}

void WorldRenderer::renderEntityNameTags(float partialTicks) {
    Minecraft *minecraft = Minecraft::getInstance();
    Font *font           = minecraft->getDefaultFont();

    for (const std::unique_ptr<Entity> &entity : m_world->getEntities()) {
        std::string strUuid   = entity->getUUID().toString();
        std::wstring wstrUuid = std::wstring(strUuid.begin(), strUuid.end());
        if (wstrUuid.empty()) continue;

        Vec3 pos     = entity->getRenderPosition(partialTicks);
        AABB box     = entity->getBoundingBox().translated(pos);
        float height = box.getMax().y - box.getMin().y;

        Vec3 textPos = pos.add(Vec3(0.0, height + 0.5, 0.0));

        GlStateManager::disableDepthTest();
        font->worldRenderShadow(wstrUuid, textPos, 0.015f, 0xFFFFFFFF);
    }
}

void WorldRenderer::renderFogPass(const Mat4 &projection) {
    GlStateManager::disableDepthTest();

    m_fogShader->bind();

    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_sceneFramebuffer->getColorTexture());
    m_fogShader->setInt("u_colorTexture", 0);

    RenderCommand::activeTexture(1);
    RenderCommand::bindTexture2D(m_sceneFramebuffer->getDepthTexture());
    m_fogShader->setInt("u_depthTexture", 1);

    m_fogShader->setInt("u_fogColormap", 2);
    ColormapManager::getInstance()->bind("fog", 2);

    Fog &fog = m_world->getFog();
    m_fogShader->setVec2("u_fogLut", fog.getLutX(), fog.getLutY());

    bool fogEnabled = GlStateManager::isFogEnabled();
    m_fogShader->setInt("u_fogEnabled", fogEnabled);

    m_fogShader->setFloat("u_fogStrength", m_world->getDaylightFactor());

    if (fogEnabled) {
        float fogStart;
        float fogEnd;
        GlStateManager::getFogRange(fogStart, fogEnd);
        m_fogShader->setVec2("u_fogRange", fogStart, fogEnd);
    }

    Minecraft *minecraft        = Minecraft::getInstance();
    Mat4 viewMatrix             = minecraft->getCamera()->getViewMatrix();
    Mat4 invertedViewProjection = projection.multiply(viewMatrix).inverse();
    m_fogShader->setMat4("u_invViewProj", invertedViewProjection.data);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);

    GlStateManager::enableDepthTest();
}

void WorldRenderer::scheduleMesher() {
    if (!m_mesherRunning || !m_mesherPool) return;

    std::vector<ChunkPos> urgent;
    std::vector<ChunkPos> normal;

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);

        while (!m_urgentQueue.empty()) {
            urgent.push_back(m_urgentQueue.front());
            m_urgentQueue.pop();
        }

        m_urgentQueued.clear();

        while (!m_rebuildQueue.empty()) {
            normal.push_back(m_rebuildQueue.front());
            m_rebuildQueue.pop();
        }

        m_rebuildQueued.clear();
    }

    for (ChunkPos pos : urgent) {
        m_mesherPool->detach_task([this, pos] {
            if (!m_mesherRunning) return;
            auto it = m_world->getChunks().find(pos);
            if (it == m_world->getChunks().end()) return;
            std::vector<ChunkMesher::MeshBuildResult> results;
            ChunkMesher::buildMeshes(m_world, it->second.get(), results);
            submitMesh(pos, std::move(results));
        });
    }

    for (ChunkPos pos : normal) {
        m_mesherPool->detach_task([this, pos] {
            if (!m_mesherRunning) return;
            auto it = m_world->getChunks().find(pos);
            if (it == m_world->getChunks().end()) return;
            std::vector<ChunkMesher::MeshBuildResult> results;
            ChunkMesher::buildMeshes(m_world, it->second.get(), results);
            submitMesh(pos, std::move(results));
        });
    }
}
