#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../entity/EntityRenderer.h"
#include "World.h"
#include "chunk/ChunkMesh.h"
#include "chunk/ChunkMesher.h"

class WorldRenderer {
public:
    explicit WorldRenderer(World *world);

    void draw();
    void drawChunkGrid() const;

    void rebuild();
    void rebuildChunk(int cx, int cy, int cz);

    void submitMesh(const ChunkPos &pos, std::vector<ChunkMesher::MeshBuildResult> &&results);

    World *m_world;

    EntityRenderer m_entityRenderer;
    bool m_drawBoundingBoxes = false;

    bool m_drawChunkGrid = false;

    struct RenderMesh {
        std::unique_ptr<ChunkMesh> mesh;
    };

    std::unordered_map<ChunkPos, std::vector<RenderMesh>, ChunkPosHash> m_chunks;
    std::mutex m_meshQueueMutex;
    std::deque<std::pair<ChunkPos, std::vector<ChunkMesher::MeshBuildResult>>> m_pendingMeshes;
};
