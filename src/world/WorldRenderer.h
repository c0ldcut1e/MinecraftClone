#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../entity/EntityRenderer.h"
#include "../rendering/Shader.h"
#include "../scene/Frustum.h"
#include "World.h"
#include "chunk/ChunkMesh.h"
#include "chunk/ChunkMesher.h"
#include "chunk/ChunkPos.h"

class WorldRenderer {
public:
    explicit WorldRenderer(World *world);
    ~WorldRenderer();

    void render(float partialTicks);

    void rebuild();
    void rebuildChunk(const ChunkPos &pos);
    void rebuildChunkUrgent(const ChunkPos &pos);

private:
    void submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results);

    void setupMatrices(const Vec3 &cameraPos);
    void setupFog();
    void renderSky(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection);
    void renderFastClouds();
    void renderClouds();
    void bindWorldShader(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection, float fogR, float fogG, float fogB);
    void uploadPendingMeshes();
    void renderChunks(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderChunkGrid() const;
    void renderEntities(float partialTicks);
    void renderEntityNameTags(float partialTicks);

    Shader *m_worldShader;
    Shader *m_skyShader;

    World *m_world;

    Frustum m_frustum;

    EntityRenderer m_entityRenderer;

    bool m_renderChunkGrid = false;
    double m_cloudOffset   = 0.0;

    struct RenderMesh {
        std::unique_ptr<ChunkMesh> mesh;
    };

    std::unordered_map<ChunkPos, std::vector<RenderMesh>, ChunkPosHash> m_chunks;
    std::mutex m_meshQueueMutex;
    std::deque<std::pair<ChunkPos, std::vector<ChunkMesher::MeshBuildResult>>> m_pendingMeshes;

    std::thread m_mesherThread;
    std::atomic<bool> m_mesherRunning{false};

    std::mutex m_rebuildQueueMutex;
    std::condition_variable m_rebuildCondition;
    std::queue<ChunkPos> m_rebuildQueue;
    std::queue<ChunkPos> m_urgentQueue;
};
