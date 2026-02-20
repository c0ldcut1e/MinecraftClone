#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <BS_thread_pool.hpp>

#include "../entity/EntityRenderer.h"
#include "../rendering/Framebuffer.h"
#include "../rendering/Shader.h"
#include "../scene/Frustum.h"
#include "World.h"
#include "chunk/ChunkMesh.h"
#include "chunk/ChunkMesher.h"
#include "chunk/ChunkPos.h"
#include "environment/CloudMesh.h"

class WorldRenderer {
public:
    explicit WorldRenderer(World *world, int width, int height);
    ~WorldRenderer();

    void render(float partialTicks);
    void onResize(int width, int height);

    void rebuild();
    void rebuildChunk(const ChunkPos &pos);
    void rebuildChunkUrgent(const ChunkPos &pos);

private:
    static std::vector<uint8_t> buildCloudMap();
    static const std::vector<uint8_t> &getCloudMap();

    void submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results);

    void setupMatrices(const Vec3 &cameraPos);
    void setupFog();
    void renderSky(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderFastClouds(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderClouds(const Mat4 &viewMatrix, const Mat4 &projection);
    void bindWorldShader(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection);
    void uploadPendingMeshes();
    void renderChunks(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderChunkGrid() const;
    void renderEntities(float partialTicks);
    void renderEntityNameTags(float partialTicks);
    void renderFogPass(const Mat4 &projection);

    void scheduleMesher();

    Shader *m_worldShader;
    Shader *m_skyShader;
    Shader *m_cloudShader;
    Shader *m_fogShader;

    Framebuffer *m_sceneFramebuffer;

    World *m_world;

    Frustum m_frustum;

    EntityRenderer m_entityRenderer;

    bool m_renderChunkGrid = false;

    CloudMesh m_cloudMesh;

    std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>, ChunkPosHash> m_chunks;
    std::mutex m_meshQueueMutex;
    std::deque<std::pair<ChunkPos, std::vector<ChunkMesher::MeshBuildResult>>> m_pendingMeshes;

    std::unique_ptr<BS::thread_pool<>> m_mesherPool;
    std::atomic<bool> m_mesherRunning{false};
    std::atomic<bool> m_mesherScheduled{false};

    std::mutex m_rebuildQueueMutex;
    std::queue<ChunkPos> m_rebuildQueue;
    std::queue<ChunkPos> m_urgentQueue;
    std::unordered_set<ChunkPos, ChunkPosHash> m_rebuildQueued;
    std::unordered_set<ChunkPos, ChunkPosHash> m_urgentQueued;
};
