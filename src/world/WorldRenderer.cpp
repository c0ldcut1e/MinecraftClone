#include "WorldRenderer.h"

#include <cmath>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../rendering/ColormapManager.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/ImmediateRenderer.h"
#include "../rendering/RenderCommand.h"
#include "../utils/Time.h"
#include "../utils/math/Math.h"
#include "models/Model.h"
#include "models/ModelDefinition.h"
#include "models/ModelPartsSkinned.h"

WorldRenderer::WorldRenderer(World *world, int width, int height) : m_worldShader(nullptr), m_skyShader(nullptr), m_cloudShader(nullptr), m_fogShader(nullptr), m_sceneFramebuffer(nullptr), m_world(world), m_renderChunkGrid(false) {
    m_worldShader = new Shader("shaders/world.vert", "shaders/world.frag");
    m_skyShader   = new Shader("shaders/sky.vert", "shaders/sky.frag");
    m_cloudShader = new Shader("shaders/cloud.vert", "shaders/cloud.frag");
    m_fogShader   = new Shader("shaders/fog.vert", "shaders/fog.frag");

    m_sceneFramebuffer = new Framebuffer(width, height);

    ColormapManager *colormapManager = ColormapManager::getInstance();
    colormapManager->load("fog", "colormap/fog.png");
    colormapManager->load("sky", "colormap/sky.png");

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

    const Vec3 cameraPos = minecraft->getCamera()->getPosition();
    Mat4 viewMatrix      = minecraft->getCamera()->getViewMatrix();
    Mat4 projection      = minecraft->getProjection();

    setupMatrices(cameraPos);
    setupFog();

    ImmediateRenderer::getForWorld()->setViewProjection(viewMatrix, projection);
    minecraft->getDefaultFont()->setNearest(true);
    minecraft->getDefaultFont()->setWorldViewProjection(viewMatrix, projection);

    m_sceneFramebuffer->bind();
    RenderCommand::clearAll();

    renderSky(viewMatrix, projection);

    bindWorldShader(cameraPos, viewMatrix, projection);
    uploadPendingMeshes();
    renderChunks(viewMatrix, projection);

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
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size());
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

    return cloudMap;
}

const std::vector<uint8_t> &WorldRenderer::getCloudMap() {
    static std::vector<uint8_t> cloudMap = buildCloudMap();
    return cloudMap;
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

    float fogStart = renderEnd * fog.getStartFactor();
    float fogEnd   = renderEnd * fog.getEndFactor();
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
    RenderCommand::enablePolygonOffsetFill();
    RenderCommand::setPolygonOffset(1.0f, 1.0f);

    m_cloudShader->bind();
    m_cloudShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_cloudShader->setMat4("u_view", viewMatrix.data);
    m_cloudShader->setMat4("u_projection", projection.data);

    m_cloudMesh.render();

    RenderCommand::disablePolygonOffsetFill();
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
}

void WorldRenderer::renderClouds(const Mat4 &viewMatrix, const Mat4 &projection) {
    Minecraft *minecraft                 = Minecraft::getInstance();
    const Vec3 cameraPos                 = minecraft->getCamera()->getPosition();
    const std::vector<uint8_t> &cloudMap = getCloudMap();

    static float cloudOffset = 0.0f;
    cloudOffset += (float) Time::getDelta() * 0.8f;
    if (cloudOffset > 3072.0f) cloudOffset -= 3072.0f;

    float renderEnd = (float) (m_world->getRenderDistance() * Chunk::SIZE_X);
    float radius    = renderEnd + 256.0f;

    const int cellSize = 12;
    const float thick  = 4.0f;
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

                pushQuad(x1, y, z1, x2, y, z1, x2, y, z2, x1, y, z2, 1.0f, 1.0f, 1.0f, 0.875f);
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

                pushQuad(x1, y, z2, x2, y, z2, x2, y, z1, x1, y, z1, 0.941f, 0.941f, 0.941f, 1.0f);
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

            if (!hasNX) pushQuad(x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, 0.847f, 0.847f, 0.847f, 1.0f);
            if (!hasPX) pushQuad(x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, 0.847f, 0.847f, 0.847f, 1.0f);
            if (!hasNZ) pushQuad(x2, y1, z1, x2, y2, z1, x1, y2, z1, x1, y1, z1, 0.847f, 0.847f, 0.847f, 1.0f);
            if (!hasPZ) pushQuad(x1, y1, z2, x1, y2, z2, x2, y2, z2, x2, y1, z2, 0.847f, 0.847f, 0.847f, 1.0f);
        }

    m_cloudMesh.upload(vertices);

    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    RenderCommand::enablePolygonOffsetFill();
    RenderCommand::setPolygonOffset(1.0f, 1.0f);

    m_cloudShader->bind();
    m_cloudShader->setMat4("u_model", GlStateManager::getMatrix().data);
    m_cloudShader->setMat4("u_view", viewMatrix.data);
    m_cloudShader->setMat4("u_projection", projection.data);

    m_cloudMesh.render();

    RenderCommand::disablePolygonOffsetFill();
    GlStateManager::disableBlend();
    GlStateManager::enableCull();
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
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size());
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
