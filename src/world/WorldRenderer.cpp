#include "WorldRenderer.h"

#include <cmath>

#include "../core/Minecraft.h"
#include "../entity/EntityRendererRegistry.h"
#include "../graphics/ImmediateRenderer.h"
#include "../graphics/RenderCommand.h"
#include "../utils/math/Math.h"

WorldRenderer::WorldRenderer(World *world) : m_world(world), m_drawBoundingBoxes(false), m_drawChunkGrid(false) {}

void WorldRenderer::draw() {
    Minecraft *minecraft = Minecraft::getInstance();
    ImmediateRenderer::getForWorld()->setViewProjection(minecraft->getCamera()->getViewMatrix(), minecraft->getProjection());

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
        for (const auto &mesh : meshes) mesh.mesh->draw();
    }

    for (const std::unique_ptr<Entity> &entity : m_world->getEntities()) {
        EntityRendererRegistry::get()->getValue(entity->getType())->draw(entity.get());
        if (m_drawBoundingBoxes) m_entityRenderer.drawBoundingBox(entity.get());
    }

    if (m_drawChunkGrid) drawChunkGrid();
}

void WorldRenderer::drawChunkGrid() const {
    if (!m_drawChunkGrid) return;

    ImmediateRenderer *renderer = ImmediateRenderer::getForWorld();
    renderer->setViewProjection(Minecraft::getInstance()->getCamera()->getViewMatrix(), Minecraft::getInstance()->getProjection());

    renderer->begin(RC_LINES);
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

void WorldRenderer::rebuildChunk(int cx, int cy, int cz) {
    ChunkPos pos{cx, cy, cz};

    auto it = m_world->getChunks().find(pos);
    if (it == m_world->getChunks().end()) return;

    std::vector<ChunkMesher::MeshBuildResult> results;
    ChunkMesher::buildMeshes(m_world, it->second.get(), results);

    std::vector<RenderMesh> meshes;
    meshes.reserve(results.size());

    for (ChunkMesher::MeshBuildResult &result : results) {
        RenderMesh renderMesh;
        renderMesh.mesh = std::make_unique<ChunkMesh>(result.texture);
        renderMesh.mesh->upload(result.vertices.data(), (uint32_t) result.vertices.size());
        meshes.push_back(std::move(renderMesh));
    }

    m_chunks[pos] = std::move(meshes);

    Logger::logInfo("Rebuilt chunk mesh (%d, %d, %d)", cx, cy, cz);
}

void WorldRenderer::submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results) {
    std::lock_guard<std::mutex> lock(m_meshQueueMutex);

    m_pendingMeshes.emplace_back(pos, std::move(results));
}
