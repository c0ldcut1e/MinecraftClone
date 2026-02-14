#include "WorldRenderer.h"

#include <cmath>
#include <thread>

#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/ImmediateRenderer.h"
#include "../utils/math/Math.h"
#include "models/Model.h"
#include "models/ModelDefinition.h"
#include "models/ModelPartsSkinned.h"

WorldRenderer::WorldRenderer(World *world) : m_shader(nullptr), m_world(world), m_drawChunkGrid(false) {
    m_shader = new Shader("shaders/world.vert", "shaders/world.frag");

    m_mesherRunning = true;
    m_mesherThread  = std::thread([this]() {
        while (m_mesherRunning) {
            ChunkPos pos;

            {
                std::unique_lock<std::mutex> lock(m_rebuildQueueMutex);
                m_rebuildCondition.wait(lock, [this]() { return !m_mesherRunning || !m_rebuildQueue.empty(); });

                if (!m_mesherRunning) break;
                if (m_rebuildQueue.empty()) continue;

                pos = m_rebuildQueue.front();
                m_rebuildQueue.pop();
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

    if (m_shader) {
        delete m_shader;
        m_shader = nullptr;
    }
}

void WorldRenderer::draw() {
    Minecraft *minecraft = Minecraft::getInstance();
    Mat4 viewMatrix      = minecraft->getCamera()->getViewMatrix();
    Mat4 projection      = minecraft->getProjection();
    ImmediateRenderer::getForWorld()->setViewProjection(viewMatrix, projection);
    minecraft->getDefaultFont()->setNearest(true);
    minecraft->getDefaultFont()->setWorldViewProjection(viewMatrix, projection);

    m_shader->bind();
    m_shader->setInt("u_texture", 0);
    m_shader->setMat4("u_model", GlStateManager::getMatrix().data());
    m_shader->setMat4("u_view", viewMatrix.data());
    m_shader->setMat4("u_projection", projection.data());

    GlStateManager::enableCull();
    GlStateManager::setFrontFace(GL_CCW);
    GlStateManager::setCullFace(GL_BACK);

    GlStateManager::enableDepthTest();

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

    Vec3 playerPos     = minecraft->getLocalPlayer()->getPosition();
    int playerChunkX   = Math::floorDiv((int) playerPos.x, Chunk::SIZE_X);
    int playerChunkZ   = Math::floorDiv((int) playerPos.z, Chunk::SIZE_Z);
    int renderDistance = m_world->getRenderDistance();
    for (const auto &[pos, meshes] : m_chunks) {
        int dx = pos.x - playerChunkX;
        int dz = pos.z - playerChunkZ;

        if (dx * dx + dz * dz > renderDistance * renderDistance) continue;
        for (const RenderMesh &mesh : meshes) mesh.mesh->draw();
    }

    for (const std::unique_ptr<Entity> &entity : m_world->getEntities()) {
        EntityRenderer *renderer = EntityRendererRegistry::get()->getValue(entity->getType());
        renderer->setDrawBoundingBox(true);
        renderer->draw(entity.get());
    }

    drawEntityNameTags();

    if (m_drawChunkGrid) drawChunkGrid();

    static TextureRepository steveTextures;

    static Model steveModel(ModelPartsSkinned::createSteveClassic("steve", "textures/steve.png"));

    GlStateManager::pushMatrix();
    GlStateManager::disableCull();

    GlStateManager::translatef(0.0f, 64.0f, 0.0f);
    GlStateManager::scalef(1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 16.0f);
    steveModel.draw(steveTextures);

    GlStateManager::popMatrix();
}

void WorldRenderer::drawChunkGrid() const {
    if (!m_drawChunkGrid) return;

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

void WorldRenderer::submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results) {
    std::lock_guard<std::mutex> lock(m_meshQueueMutex);

    m_pendingMeshes.emplace_back(pos, std::move(results));
}

void WorldRenderer::drawEntityNameTags() {
    Minecraft *minecraft = Minecraft::getInstance();
    Font *font           = minecraft->getDefaultFont();

    for (const std::unique_ptr<Entity> &entity : m_world->getEntities()) {
        std::string strUuid   = entity->getUUID().toString();
        std::wstring wstrUuid = std::wstring(strUuid.begin(), strUuid.end());
        if (wstrUuid.empty()) continue;

        const AABB &box = entity->getAABB();
        float height    = box.getMax().y - box.getMin().y;

        Vec3 pos     = entity->getPosition();
        Vec3 textPos = pos.add(Vec3(0.0, height + 0.5, 0.0));

        font->worldDrawShadow(wstrUuid, textPos, 0.015f, 0xFFFFFFFF);
    }
}
