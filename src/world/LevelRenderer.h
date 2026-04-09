#pragma once

#include <atomic>
#include <climits>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../entity/EntityRenderer.h"
#include "../rendering/Framebuffer.h"
#include "../rendering/Shader.h"
#include "../scene/culling/FrustumCuller.h"
#include "../threading/ThreadPool.h"
#include "Level.h"
#include "chunk/ChunkMesh.h"
#include "chunk/ChunkMesher.h"
#include "chunk/ChunkPos.h"
#include "environment/CloudMesh.h"
#include "lighting/LightCache.h"
#include "lighting/LightStorage.h"
#include "lighting/dynamic/DynamicLightPipeline.h"

class LevelRenderer
{
public:
    enum class LightingMode
    {
        OLD,
        NEW,
        OFF
    };

    enum class BlockOutlineMode
    {
        NORMAL,
        BEDROCK
    };

    explicit LevelRenderer(Level *level, int width, int height);
    ~LevelRenderer();

    void render(float partialTicks, Framebuffer *outputFramebuffer = nullptr);
    void onResize(int width, int height);
    void renderDynamicLightImGui();

    void rebuild();
    void rebuildChunk(const ChunkPos &pos);
    void rebuildChunkUrgent(const ChunkPos &pos);
    void cycleLightingMode();
    void cycleBlockOutlineMode();
    void toggleGrassSideOverlay();
    void toggleDynamicLightPipeline();
    LightingMode getLightingMode() const;
    BlockOutlineMode getBlockOutlineMode() const;
    bool isGrassSideOverlayEnabled() const;
    bool isDynamicLightPipelineEnabled() const;
    size_t getMeshedChunkCount() const;
    size_t getVisibleChunkCount() const;
    size_t getRenderedMeshCount() const;
    size_t getPendingMeshCount() const;
    size_t getQueuedRebuildCount() const;
    size_t getUrgentRebuildCount() const;
    size_t getSkyQueueCount() const;
    size_t getMesherThreadCount() const;

private:
    struct VisibleChunk
    {
        const ChunkPos *pos;
        const std::vector<std::unique_ptr<ChunkMesh>> *meshes;
        float alpha;
        double distanceSquared;
    };

    struct ChunkFadeSettings
    {
        float fadeInDuration;
        float reappearDelay;
    };

    struct ChunkFadeState
    {
        float alpha{1.0f};
        float hiddenTime{0.0f};
        bool visible{false};
        bool hasRendered{false};
        bool pendingFade{false};
    };

    struct SkyUpdateResult
    {
        ChunkPos pos;
        size_t index;
        uint64_t meshId;
        std::vector<uint8_t> lightData;
    };

    struct PendingMeshUpload
    {
        ChunkPos pos;
        uint64_t generation;
        std::vector<ChunkMesher::MeshBuildResult> results;
    };

    struct ReadyMeshSwap
    {
        uint64_t generation;
        bool hadMeshes;
        std::vector<std::unique_ptr<ChunkMesh>> meshes;
    };

    static std::vector<uint8_t> buildCloudMap();
    static const std::vector<uint8_t> &getCloudMap();

    static std::vector<float> buildStarVertices();

    void submitMesh(const ChunkPos &pos, uint64_t generation,
                    std::vector<ChunkMesher::MeshBuildResult> &&results);

    void setupMatrices(const Vec3 &cameraPos);
    void setupFog();
    void renderSky(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderFastClouds(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderClouds(const Mat4 &viewMatrix, const Mat4 &projection);
    void renderStars(const Mat4 &viewMatrix, const Mat4 &projection);
    void bindLevelShader(const Vec3 &cameraPos, const Mat4 &viewMatrix, const Mat4 &projection);
    void uploadPendingMeshes();
    void presentReadyMeshes();
    void collectVisibleChunks(const Vec3 &playerPos, const Vec3 &cameraPos, const Mat4 &viewMatrix,
                              const Mat4 &projection, std::vector<VisibleChunk> *opaqueChunks,
                              std::vector<VisibleChunk> *fadingChunks);
    void renderChunks(Shader *shader, const std::vector<VisibleChunk> &opaqueChunks,
                      std::vector<VisibleChunk> fadingChunks);
    void renderDynamicLightBuffers(const Vec3 &cameraPos, const Mat4 &viewMatrix,
                                   const Mat4 &projection,
                                   const std::vector<VisibleChunk> &opaqueChunks,
                                   const std::vector<VisibleChunk> &fadingChunks);
    void renderRenderObjects(LevelRenderObjectStage stage);
    void renderParticles(float partialTicks);
    void renderChunkGrid() const;
    void renderBlockOutline();
    void renderEntities(float partialTicks);
    void renderEntityNameTags(float partialTicks);
    void renderFogPass(const Mat4 &projection, Framebuffer *sourceFramebuffer);

    void scheduleMesher();

    void updateLightState(const DimensionTime &dimensionTime);
    void beginSkyLightClampUpdate(const DimensionTime &dimensionTime);
    void pumpSkyLightClampUpdate(int scheduleBudget, int applyBudget);
    float getChunkFadeAlpha(const ChunkPos &pos, float delta);
    void tickHiddenChunkFadeState(const ChunkPos &pos, float delta);
    Vec3 sampleDynamicLightRgb(const Vec3 &samplePos) const;
    uint32_t sampleWorldLightColor(const Vec3 &samplePos) const;
    uint32_t sampleEntityLightColor(const Entity *entity, float partialTicks) const;

    Shader *m_levelShader;
    Shader *m_skyShader;
    Shader *m_cloudShader;
    Shader *m_fogShader;
    Shader *m_starShader;

    uint32_t m_starVao;
    uint32_t m_starVbo;
    int m_starVertexCount;

    Framebuffer *m_sceneFramebuffer;
    class DynamicLightPipeline *m_dynamicLightPipeline;

    Level *m_level;

    FrustumCuller m_frustum;

    EntityRenderer m_entityRenderer;

    bool m_renderChunkGrid;

    CloudMesh m_cloudMesh;
    float m_cloudOffset;
    int m_cloudLastCamCellX;
    int m_cloudLastCamCellZ;
    float m_cloudLastBuiltOffset;
    float m_cloudLightSmooth;

    uint8_t m_lastSkyLightClamp{255};
    LightStorage m_lightStorage;
    LightCache m_lightCache;
    std::vector<DynamicLightPipeline::PackedLight> m_forwardDynamicLights;

    std::mutex m_skyMutex;
    std::deque<SkyUpdateResult> m_skyResults;
    std::deque<ChunkPos> m_skyQueue;
    std::unordered_set<ChunkPos, ChunkPosHash> m_skyQueued;
    uint8_t m_skyClampTarget{15};

    std::unordered_map<ChunkPos, std::vector<std::unique_ptr<ChunkMesh>>, ChunkPosHash> m_chunks;
    std::unordered_map<ChunkPos, ChunkFadeState, ChunkPosHash> m_chunkFadeStates;
    ChunkFadeSettings m_chunkFadeSettings{0.20f, 0.12f};
    size_t m_lastVisibleChunkCount{0};
    size_t m_lastRenderedMeshCount{0};
    mutable std::mutex m_meshQueueMutex;
    std::deque<PendingMeshUpload> m_pendingMeshes;
    std::unordered_map<ChunkPos, ReadyMeshSwap, ChunkPosHash> m_readyMeshes;

    std::unique_ptr<ThreadPool> m_mesherPool;
    std::atomic<bool> m_mesherRunning{false};
    std::atomic<int> m_activeMesherTasks{0};
    size_t m_maxMesherTasks{1};

    LightingMode m_lightingMode;
    bool m_grassSideOverlayEnabled;
    BlockOutlineMode m_blockOutlineMode;

    mutable std::mutex m_rebuildQueueMutex;
    std::queue<ChunkPos> m_rebuildQueue;
    std::queue<ChunkPos> m_urgentQueue;
    std::unordered_set<ChunkPos, ChunkPosHash> m_rebuildQueued;
    std::unordered_set<ChunkPos, ChunkPosHash> m_urgentQueued;
    std::unordered_set<ChunkPos, ChunkPosHash> m_activeRebuilds;
    std::unordered_set<ChunkPos, ChunkPosHash> m_deferredRebuilds;
    std::unordered_set<ChunkPos, ChunkPosHash> m_deferredUrgentRebuilds;
    std::unordered_map<ChunkPos, uint64_t, ChunkPosHash> m_requestedMeshGenerations;
};
