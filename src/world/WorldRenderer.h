#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
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

    void draw(float alpha);
    void drawChunkGrid() const;

    void rebuild();
    void rebuildChunk(const ChunkPos &pos);

private:
    void submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results);

    void drawEntityNameTags(float alpha);

    Shader *m_shader;

    World *m_world;

    Frustum m_frustum;

    EntityRenderer m_entityRenderer;

    bool m_drawChunkGrid = false;

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
};
