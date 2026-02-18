#include "WorldRenderer.h"

#include <cmath>
#include <thread>

#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/ImmediateRenderer.h"
#include "../rendering/RenderCommand.h"
#include "../utils/Time.h"
#include "../utils/math/Math.h"
#include "models/Model.h"
#include "models/ModelDefinition.h"
#include "models/ModelPartsSkinned.h"

WorldRenderer::WorldRenderer(World *world) : m_worldShader(nullptr), m_skyShader(nullptr), m_world(world), m_renderChunkGrid(false) {
    m_worldShader = new Shader("shaders/world.vert", "shaders/world.frag");
    m_skyShader   = new Shader("shaders/sky.vert", "shaders/sky.frag");

    m_mesherRunning = true;
    m_mesherThread  = std::thread([this]() {
        while (m_mesherRunning) {
            ChunkPos pos;

            {
                std::unique_lock<std::mutex> lock(m_rebuildQueueMutex);
                m_rebuildCondition.wait(lock, [this]() { return !m_mesherRunning || !m_urgentQueue.empty() || !m_rebuildQueue.empty(); });

                if (!m_mesherRunning) break;

                if (!m_urgentQueue.empty()) {
                    pos = m_urgentQueue.front();
                    m_urgentQueue.pop();
                } else if (!m_rebuildQueue.empty()) {
                    pos = m_rebuildQueue.front();
                    m_rebuildQueue.pop();
                } else {
                    continue;
                }
            }

            auto it = m_world->getChunks().find(pos);
            if (it == m_world->getChunks().end()) continue;

            std::vector<ChunkMesher::MeshBuildResult> results;
            ChunkMesher::buildMeshes(m_world, it->second.get(), results);

            submitMesh(pos, std::move(results));
        }
    });
}

WorldRenderer::~WorldRenderer() {
    m_mesherRunning = false;
    m_rebuildCondition.notify_all();
    if (m_mesherThread.joinable()) m_mesherThread.join();

    if (m_skyShader) {
        delete m_skyShader;
        m_skyShader = nullptr;
    }

    if (m_worldShader) {
        delete m_worldShader;
        m_worldShader = nullptr;
    }
}

void WorldRenderer::render(float partialTicks) {
    Minecraft *minecraft = Minecraft::getInstance();

    const Vec3 cameraPos = minecraft->getCamera()->getPosition();
    Mat4 viewMatrix      = minecraft->getCamera()->getViewMatrix();
    Mat4 projection      = minecraft->getProjection();

    setupMatrices(cameraPos);

    ImmediateRenderer::getForWorld()->setViewProjection(viewMatrix, projection);
    minecraft->getDefaultFont()->setNearest(true);
    minecraft->getDefaultFont()->setWorldViewProjection(viewMatrix, projection);

    setupFog();

    float fogR;
    float fogG;
    float fogB;
    GlStateManager::getFogColor(fogR, fogG, fogB);

    renderSky(cameraPos, viewMatrix, projection);
    renderClouds();

    bindWorldShader(cameraPos, viewMatrix, projection, fogR, fogG, fogB);
    uploadPendingMeshes();

    renderChunks(viewMatrix, projection);
    if (m_renderChunkGrid) renderChunkGrid();

    renderEntities(partialTicks);
    renderEntityNameTags(partialTicks);

    GlStateManager::popMatrix();
}

void WorldRenderer::rebuild() {
    m_chunks.clear();

    for (const auto &[pos, chunkPtr] : m_world->getChunks()) {
        std::vector<ChunkMesher::MeshBuildResult> results;
        ChunkMesher::buildMeshes(m_world, chunkPtr.get(), results);

        std::vector<RenderMesh> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results) {
            RenderMesh renderMesh;
            renderMesh.mesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh.mesh->upload(result.vertices.data(), (uint32_t) result.vertices.size());
            meshes.push_back(std::move(renderMesh));
        }

        m_chunks.emplace(pos, std::move(meshes));
    }
}

void WorldRenderer::rebuildChunk(const ChunkPos &pos) {
    auto it = m_world->getChunks().find(pos);
    if (it == m_world->getChunks().end()) return;

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
        m_rebuildQueue.push(pos);
    }

    m_rebuildCondition.notify_one();
}

void WorldRenderer::rebuildChunkUrgent(const ChunkPos &pos) {
    auto it = m_world->getChunks().find(pos);
    if (it == m_world->getChunks().end()) return;

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
        m_urgentQueue.push(pos);
    }

    m_rebuildCondition.notify_one();
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
    GlStateManager::enableFog();
    GlStateManager::setFogColor(0.435294118f, 0.709803922f, 0.97254902f);

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
    float fogEnd    = renderEnd * 0.65f;
    float fogStart  = fogEnd * 0.08f;

    GlStateManager::setFogRange(fogStart, fogEnd);
}

void WorldRenderer::renderSky(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection) {
    GlStateManager::disableDepthTest();

    m_skyShader->bind();
    m_skyShader->setVec3("u_skyColor", 0.535294118f, 0.809803922f, 0.97254902f);

    float fogR;
    float fogG;
    float fogB;

    GlStateManager::getFogColor(fogR, fogG, fogB);
    m_skyShader->setVec3("u_fogColor", fogR, fogG, fogB);

    m_skyShader->setInt("u_fogEnabled", GlStateManager::isFogEnabled() ? 1 : 0);

    Mat4 invViewProj = projection.multiply(viewMatrix).inverse();
    m_skyShader->setMat4("u_invViewProj", invViewProj.data());
    m_skyShader->setVec3("u_cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

    RenderCommand::renderArrays(GL_TRIANGLES, 0, 3);

    GlStateManager::enableDepthTest();
}

void WorldRenderer::renderFastClouds() {
    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 cameraPos = minecraft->getCamera()->getPosition();

    static bool inited = false;
    static std::vector<uint8_t> cloudMap;
    static float cloudOffset = 0.0f;

    if (!inited) {
        cloudMap.resize(256 * 256);

        for (int z = 0; z < 256; z++)
            for (int x = 0; x < 256; x++) {
                uint32_t height                                  = (uint32_t) (x * 3129871) ^ (uint32_t) (z * 116129781);
                height                                           = height * height * 42317861u + height * 11u;
                cloudMap[(size_t) x + (size_t) 256 * (size_t) z] = ((height >> 16) & 15) < 8 ? 1 : 0;
            }

        std::vector<uint8_t> temp;
        temp.resize(256 * 256);

        for (int pass = 0; pass < 3; pass++) {
            for (int z = 0; z < 256; z++)
                for (int x = 0; x < 256; x++) {
                    int c = 0;
                    for (int dz = -1; dz <= 1; dz++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = (x + dx) & 255;
                            int nz = (z + dz) & 255;
                            c += cloudMap[(size_t) nx + (size_t) 256 * (size_t) nz] ? 1 : 0;
                        }

                    temp[(size_t) x + (size_t) 256 * (size_t) z] = c >= 5 ? 1 : 0;
                }

            cloudMap.swap(temp);
        }

        inited = true;
    }

    cloudOffset += (float) Time::getDelta() * 0.60f;
    if (cloudOffset > 3072.0f) cloudOffset -= 3072.0f;

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    float cloudY       = 128.0f;

    int camCellX = (int) floor((cameraPos.x + cloudOffset) / (float) cellSize);
    int camCellZ = (int) floor(cameraPos.z / (float) cellSize);

    int radiusCells = (int) ceil(radius / (float) cellSize);

    int width  = radiusCells * 2 + 1;
    int height = radiusCells * 2 + 1;

    std::vector<uint8_t> mask;
    mask.resize((size_t) width * (size_t) height);

    for (int z = 0; z < height; z++)
        for (int x = 0; x < width; x++) {
            int wx = camCellX + (x - radiusCells);
            int wz = camCellZ + (z - radiusCells);

            uint8_t v                                      = cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
            mask[(size_t) x + (size_t) width * (size_t) z] = v;
        }

    GlStateManager::disableCull();

    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->begin(GL_TRIANGLES);
    renderer->color(0xDFFFFFFF);

    float baseX = (float) (camCellX - radiusCells) * (float) cellSize - cloudOffset;
    float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

    float fogR;
    float fogG;
    float fogB;
    GlStateManager::getFogColor(fogR, fogG, fogB);

    float fogStart;
    float fogEnd;
    GlStateManager::getFogRange(fogStart, fogEnd);

    float fogColR = fogR + (1.0f - fogR) * 0.78f;
    float fogColG = fogG + (1.0f - fogG) * 0.78f;
    float fogColB = fogB + (1.0f - fogB) * 0.78f;

    auto smoothstep01 = [](float x) {
        x = Math::clampf(x, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };

    auto linearFog = [&](float d) {
        float s = fogStart / 3.0f;
        if (d <= s) return 0.0f;
        if (d >= fogEnd) return 1.0f;
        return (d - s) / (fogEnd - s);
    };

    auto fogValueAt = [&](float wx, float wy, float wz) {
        float dx = wx - (float) cameraPos.x;
        float dy = wy - (float) cameraPos.y;
        float dz = wz - (float) cameraPos.z;

        float distS  = sqrtf(dx * dx + dy * dy + dz * dz);
        float distXZ = sqrtf(dx * dx + dz * dz);
        float distC  = fmaxf(distXZ, fabsf(dy));

        float a = linearFog(distS);
        float b = linearFog(distC);
        float v = fmaxf(a, b);

        v = smoothstep01(v);
        v = powf(v, 2.4f);
        return Math::clampf(v, 0.0f, 1.0f);
    };

    auto pack = [&](int a, int r, int g, int b) -> uint32_t { return ((uint32_t) (a & 255) << 24) | ((uint32_t) (r & 255) << 16) | ((uint32_t) (g & 255) << 8) | (uint32_t) (b & 255); };

    for (int z = 0; z < height; z++)
        for (int x = 0; x < width; x++) {
            if (!mask[(size_t) x + (size_t) width * (size_t) z]) continue;

            int rw = 1;
            while (x + rw < width && mask[(size_t) (x + rw) + (size_t) width * (size_t) z]) rw++;

            int rh  = 1;
            bool ok = true;
            while (z + rh < height && ok) {
                for (int k = 0; k < rw; k++)
                    if (!mask[(size_t) (x + k) + (size_t) width * (size_t) (z + rh)]) {
                        ok = false;
                        break;
                    }

                if (ok) rh++;
            }

            for (int zz = 0; zz < rh; zz++)
                for (int xx = 0; xx < rw; xx++) mask[(size_t) (x + xx) + (size_t) width * (size_t) (z + zz)] = 0;

            float x1 = baseX + (float) x * (float) cellSize;
            float z1 = baseZ + (float) z * (float) cellSize;
            float x2 = baseX + (float) (x + rw) * (float) cellSize;
            float z2 = baseZ + (float) (z + rh) * (float) cellSize;

            renderer->vertex(x1, cloudY, z1);
            renderer->vertex(x2, cloudY, z1);
            renderer->vertex(x2, cloudY, z2);

            renderer->vertex(x1, cloudY, z1);
            renderer->vertex(x2, cloudY, z2);
            renderer->vertex(x1, cloudY, z2);
        }

    renderer->end();
    GlStateManager::disableBlend();
}

void WorldRenderer::renderClouds() {
    Minecraft *minecraft = Minecraft::getInstance();
    const Vec3 cameraPos = minecraft->getCamera()->getPosition();

    static bool inited = false;
    static std::vector<uint8_t> cloudMap;
    static float cloudOffset = 0.0f;

    if (!inited) {
        cloudMap.resize(256 * 256);

        for (int z = 0; z < 256; z++)
            for (int x = 0; x < 256; x++) {
                uint32_t h                                       = (uint32_t) (x * 3129871) ^ (uint32_t) (z * 116129781);
                h                                                = h * h * 42317861u + h * 11u;
                cloudMap[(size_t) x + (size_t) 256 * (size_t) z] = ((h >> 16) & 15) < 8 ? 1 : 0;
            }

        std::vector<uint8_t> temp;
        temp.resize(256 * 256);

        for (int pass = 0; pass < 3; pass++) {
            for (int z = 0; z < 256; z++)
                for (int x = 0; x < 256; x++) {
                    int c = 0;
                    for (int dz = -1; dz <= 1; dz++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = (x + dx) & 255;
                            int nz = (z + dz) & 255;
                            c += cloudMap[(size_t) nx + (size_t) 256 * (size_t) nz] ? 1 : 0;
                        }

                    temp[(size_t) x + (size_t) 256 * (size_t) z] = c >= 5 ? 1 : 0;
                }

            cloudMap.swap(temp);
        }

        inited = true;
    }

    cloudOffset += (float) Time::getDelta() * 0.60f;
    if (cloudOffset > 3072.0f) cloudOffset -= 3072.0f;

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    const float thick  = 4.0f;
    float cloudY       = 128.0f;

    int camCellX = (int) floor((cameraPos.x + cloudOffset) / (float) cellSize);
    int camCellZ = (int) floor(cameraPos.z / (float) cellSize);

    int radiusCells = (int) ceil(radius / (float) cellSize);

    int width  = radiusCells * 2 + 1;
    int height = radiusCells * 2 + 1;

    std::vector<uint8_t> mask;
    mask.resize((size_t) width * (size_t) height);

    for (int z = 0; z < height; z++)
        for (int x = 0; x < width; x++) {
            int wx = camCellX + (x - radiusCells);
            int wz = camCellZ + (z - radiusCells);

            uint8_t v                                      = cloudMap[(size_t) (wx & 255) + (size_t) 256 * (size_t) (wz & 255)];
            mask[(size_t) x + (size_t) width * (size_t) z] = v;
        }

    GlStateManager::disableCull();

    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();

    float baseX = (float) (camCellX - radiusCells) * (float) cellSize - cloudOffset;
    float baseZ = (float) (camCellZ - radiusCells) * (float) cellSize;

    {
        std::vector<uint8_t> topMask = mask;

        renderer->begin(GL_TRIANGLES);
        renderer->color(0xDFFFFFFF);

        for (int z = 0; z < height; z++)
            for (int x = 0; x < width; x++) {
                if (!topMask[(size_t) x + (size_t) width * (size_t) z]) continue;

                int rw = 1;
                while (x + rw < width && topMask[(size_t) (x + rw) + (size_t) width * (size_t) z]) rw++;

                int rh  = 1;
                bool ok = true;
                while (z + rh < height && ok) {
                    for (int k = 0; k < rw; k++)
                        if (!topMask[(size_t) (x + k) + (size_t) width * (size_t) (z + rh)]) {
                            ok = false;
                            break;
                        }

                    if (ok) rh++;
                }

                for (int zz = 0; zz < rh; zz++)
                    for (int xx = 0; xx < rw; xx++) topMask[(size_t) (x + xx) + (size_t) width * (size_t) (z + zz)] = 0;

                float x1 = baseX + (float) x * (float) cellSize;
                float z1 = baseZ + (float) z * (float) cellSize;
                float x2 = baseX + (float) (x + rw) * (float) cellSize;
                float z2 = baseZ + (float) (z + rh) * (float) cellSize;

                float y = cloudY + thick;

                renderer->vertex(x1, y, z1);
                renderer->vertex(x2, y, z1);
                renderer->vertex(x2, y, z2);

                renderer->vertex(x1, y, z1);
                renderer->vertex(x2, y, z2);
                renderer->vertex(x1, y, z2);
            }

        renderer->end();
    }

    {
        std::vector<uint8_t> botMask = mask;

        GlStateManager::disableBlend();

        renderer->begin(GL_TRIANGLES);
        renderer->color(0xFFF0F0F0);

        for (int z = 0; z < height; z++)
            for (int x = 0; x < width; x++) {
                if (!botMask[(size_t) x + (size_t) width * (size_t) z]) continue;

                int rw = 1;
                while (x + rw < width && botMask[(size_t) (x + rw) + (size_t) width * (size_t) z]) rw++;

                int rh  = 1;
                bool ok = true;
                while (z + rh < height && ok) {
                    for (int k = 0; k < rw; k++)
                        if (!botMask[(size_t) (x + k) + (size_t) width * (size_t) (z + rh)]) {
                            ok = false;
                            break;
                        }

                    if (ok) rh++;
                }

                for (int zz = 0; zz < rh; zz++)
                    for (int xx = 0; xx < rw; xx++) botMask[(size_t) (x + xx) + (size_t) width * (size_t) (z + zz)] = 0;

                float x1 = baseX + (float) x * (float) cellSize;
                float z1 = baseZ + (float) z * (float) cellSize;
                float x2 = baseX + (float) (x + rw) * (float) cellSize;
                float z2 = baseZ + (float) (z + rh) * (float) cellSize;

                float y = cloudY;

                renderer->vertex(x1, y, z2);
                renderer->vertex(x2, y, z2);
                renderer->vertex(x2, y, z1);

                renderer->vertex(x1, y, z2);
                renderer->vertex(x2, y, z1);
                renderer->vertex(x1, y, z1);
            }

        renderer->end();

        GlStateManager::enableBlend();
        GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    {
        renderer->begin(GL_TRIANGLES);
        renderer->color(0xD8FFFFFF);

        for (int z = 0; z < height; z++)
            for (int x = 0; x < width; x++) {
                if (!mask[(size_t) x + (size_t) width * (size_t) z]) continue;

                bool nx0 = (x == 0) ? false : (mask[(size_t) (x - 1) + (size_t) width * (size_t) z] != 0);
                bool nx1 = (x == width - 1) ? false : (mask[(size_t) (x + 1) + (size_t) width * (size_t) z] != 0);
                bool nz0 = (z == 0) ? false : (mask[(size_t) x + (size_t) width * (size_t) (z - 1)] != 0);
                bool nz1 = (z == height - 1) ? false : (mask[(size_t) x + (size_t) width * (size_t) (z + 1)] != 0);

                float x1 = baseX + (float) x * (float) cellSize;
                float z1 = baseZ + (float) z * (float) cellSize;
                float x2 = x1 + (float) cellSize;
                float z2 = z1 + (float) cellSize;

                float y1 = cloudY;
                float y2 = cloudY + thick;

                if (!nx0) {
                    renderer->vertex(x1, y1, z2);
                    renderer->vertex(x1, y2, z2);
                    renderer->vertex(x1, y2, z1);

                    renderer->vertex(x1, y1, z2);
                    renderer->vertex(x1, y2, z1);
                    renderer->vertex(x1, y1, z1);
                }

                if (!nx1) {
                    renderer->vertex(x2, y1, z1);
                    renderer->vertex(x2, y2, z1);
                    renderer->vertex(x2, y2, z2);

                    renderer->vertex(x2, y1, z1);
                    renderer->vertex(x2, y2, z2);
                    renderer->vertex(x2, y1, z2);
                }

                if (!nz0) {
                    renderer->vertex(x2, y1, z1);
                    renderer->vertex(x2, y2, z1);
                    renderer->vertex(x1, y2, z1);

                    renderer->vertex(x2, y1, z1);
                    renderer->vertex(x1, y2, z1);
                    renderer->vertex(x1, y1, z1);
                }

                if (!nz1) {
                    renderer->vertex(x1, y1, z2);
                    renderer->vertex(x1, y2, z2);
                    renderer->vertex(x2, y2, z2);

                    renderer->vertex(x1, y1, z2);
                    renderer->vertex(x2, y2, z2);
                    renderer->vertex(x2, y1, z2);
                }
            }

        renderer->end();
    }

    GlStateManager::disableBlend();
}

void WorldRenderer::bindWorldShader(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection, float fogR, float fogG, float fogB) {
    m_worldShader->bind();
    m_worldShader->setInt("u_texture", 0);

    m_worldShader->setMat4("u_model", GlStateManager::getMatrix().data());
    m_worldShader->setMat4("u_view", viewMatrix.data());
    m_worldShader->setMat4("u_projection", projection.data());
    m_worldShader->setVec3("u_cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

    bool fogEnabled = GlStateManager::isFogEnabled();
    m_worldShader->setInt("u_fogEnabled", fogEnabled);

    if (fogEnabled) {
        float _fogStart;
        float _fogEnd;

        GlStateManager::getFogRange(_fogStart, _fogEnd);

        m_worldShader->setVec3("u_fogColor", fogR, fogG, fogB);
        m_worldShader->setVec2("u_fogRange", _fogStart, _fogEnd);
    }

    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    GlStateManager::enableDepthTest();
}

void WorldRenderer::uploadPendingMeshes() {
    int uploadsPerFrame = 2;

    while (uploadsPerFrame-- > 0) {
        std::lock_guard<std::mutex> lock(m_meshQueueMutex);

        if (m_pendingMeshes.empty()) break;

        auto pair = std::move(m_pendingMeshes.front());
        m_pendingMeshes.pop_front();

        const ChunkPos &pos                                = pair.first;
        std::vector<ChunkMesher::MeshBuildResult> &results = pair.second;

        std::vector<RenderMesh> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results) {
            RenderMesh renderMesh;
            renderMesh.mesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh.mesh->upload(result.vertices.data(), (uint32_t) result.vertices.size());
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

        for (const RenderMesh &mesh : meshes) mesh.mesh->render();
    }
}

void WorldRenderer::renderChunkGrid() const {
    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->setViewProjection(Minecraft::getInstance()->getCamera()->getViewMatrix(), Minecraft::getInstance()->getProjection());

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
