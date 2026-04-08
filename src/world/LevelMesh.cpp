#include "LevelRenderer.h"

#include "../threading/ThreadStorage.h"
#include "lighting/Lighting.h"

void LevelRenderer::rebuild()
{
    m_chunks.clear();
    m_requestedMeshGenerations.clear();
    m_readyMeshes.clear();
    bool smoothLighting   = Lighting::isOn() && m_lightingMode == LightingMode::NEW;
    bool grassSideOverlay = m_grassSideOverlayEnabled;

    const std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> &chunks =
            m_level->getChunks();
    std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash>::const_iterator chunkIt;
    for (chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt)
    {
        const ChunkPos &pos                    = chunkIt->first;
        const std::unique_ptr<Chunk> &chunkPtr = chunkIt->second;
        std::vector<ChunkMesher::MeshBuildResult> results;
        ChunkMesher::buildMeshes(m_level, chunkPtr.get(), smoothLighting, grassSideOverlay,
                                 &results);

        std::vector<std::unique_ptr<ChunkMesh>> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results)
        {
            std::unique_ptr<ChunkMesh> renderMesh;
            renderMesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size(),
                               std::move(result.atlasRects), std::move(result.rawLights),
                               std::move(result.shades), std::move(result.tints));
            meshes.push_back(std::move(renderMesh));
        }

        m_chunks.emplace(pos, std::move(meshes));

        ChunkFadeState &fadeState = m_chunkFadeStates[pos];
        if (fadeState.hasRendered)
        {
            fadeState.alpha       = 1.0f;
            fadeState.pendingFade = false;
        }
        else
        {
            fadeState.pendingFade = true;
        }
    }
}

void LevelRenderer::rebuildChunk(const ChunkPos &pos)
{
    if (m_level->getChunks().find(pos) == m_level->getChunks().end())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
        m_requestedMeshGenerations[pos]++;

        if (m_activeRebuilds.find(pos) != m_activeRebuilds.end())
        {
            if (m_deferredUrgentRebuilds.find(pos) == m_deferredUrgentRebuilds.end())
            {
                m_deferredRebuilds.insert(pos);
            }
            return;
        }

        if (m_rebuildQueued.insert(pos).second)
        {
            m_rebuildQueue.push(pos);
        }
    }

    scheduleMesher();
}

void LevelRenderer::rebuildChunkUrgent(const ChunkPos &pos)
{
    if (m_level->getChunks().find(pos) == m_level->getChunks().end())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
        m_requestedMeshGenerations[pos]++;

        if (m_activeRebuilds.find(pos) != m_activeRebuilds.end())
        {
            m_deferredRebuilds.erase(pos);
            m_deferredUrgentRebuilds.insert(pos);
            return;
        }

        if (m_urgentQueued.insert(pos).second)
        {
            m_urgentQueue.push(pos);
        }
    }

    scheduleMesher();
}

size_t LevelRenderer::getPendingMeshCount() const
{
    std::lock_guard<std::mutex> lock(m_meshQueueMutex);
    return m_pendingMeshes.size();
}

size_t LevelRenderer::getQueuedRebuildCount() const
{
    std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
    return m_rebuildQueued.size();
}

size_t LevelRenderer::getUrgentRebuildCount() const
{
    std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
    return m_urgentQueued.size();
}

void LevelRenderer::uploadPendingMeshes()
{
    int uploadsPerFrame = m_lightingMode == LightingMode::NEW && Lighting::isOn() ? 4 : 16;
    while (uploadsPerFrame-- > 0)
    {
        PendingMeshUpload pendingUpload;

        {
            std::lock_guard<std::mutex> lock(m_meshQueueMutex);

            if (m_pendingMeshes.empty())
            {
                break;
            }

            pendingUpload = std::move(m_pendingMeshes.front());
            m_pendingMeshes.pop_front();
        }

        const ChunkPos &pos = pendingUpload.pos;

        {
            std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
            auto generationIt = m_requestedMeshGenerations.find(pos);
            if (generationIt != m_requestedMeshGenerations.end() &&
                pendingUpload.generation != generationIt->second)
            {
                continue;
            }
        }

        std::vector<ChunkMesher::MeshBuildResult> &results = pendingUpload.results;
        bool hadMeshes                                     = m_chunks.find(pos) != m_chunks.end();

        std::vector<std::unique_ptr<ChunkMesh>> meshes;
        meshes.reserve(results.size());

        for (ChunkMesher::MeshBuildResult &result : results)
        {
            std::unique_ptr<ChunkMesh> renderMesh;
            renderMesh = std::make_unique<ChunkMesh>(result.texture);
            renderMesh->upload(result.vertices.data(), (uint32_t) result.vertices.size(),
                               std::move(result.atlasRects), std::move(result.rawLights),
                               std::move(result.shades), std::move(result.tints));
            meshes.push_back(std::move(renderMesh));
        }

        m_readyMeshes[pos] = {pendingUpload.generation, hadMeshes, std::move(meshes)};
    }

    presentReadyMeshes();
}

void LevelRenderer::presentReadyMeshes()
{
    std::lock_guard<std::mutex> rebuildLock(m_rebuildQueueMutex);

    for (auto it = m_readyMeshes.begin(); it != m_readyMeshes.end();)
    {
        const ChunkPos &pos        = it->first;
        ReadyMeshSwap &readyMeshes = it->second;

        auto generationIt = m_requestedMeshGenerations.find(pos);
        if (generationIt != m_requestedMeshGenerations.end() &&
            readyMeshes.generation != generationIt->second)
        {
            it = m_readyMeshes.erase(it);
            continue;
        }

        bool shouldWait = readyMeshes.hadMeshes;
        if (shouldWait)
        {
            shouldWait                      = false;
            static const ChunkPos offsets[] = {
                    ChunkPos(-1, 0, 0), ChunkPos(1, 0, 0),  ChunkPos(0, -1, 0),
                    ChunkPos(0, 1, 0),  ChunkPos(0, 0, -1), ChunkPos(0, 0, 1),
            };

            for (const ChunkPos &offset : offsets)
            {
                ChunkPos neighbor = pos + offset;
                if (m_activeRebuilds.find(neighbor) != m_activeRebuilds.end() ||
                    m_rebuildQueued.find(neighbor) != m_rebuildQueued.end() ||
                    m_urgentQueued.find(neighbor) != m_urgentQueued.end() ||
                    m_deferredRebuilds.find(neighbor) != m_deferredRebuilds.end() ||
                    m_deferredUrgentRebuilds.find(neighbor) != m_deferredUrgentRebuilds.end())
                {
                    shouldWait = true;
                    break;
                }
            }
        }

        if (shouldWait)
        {
            ++it;
            continue;
        }

        bool hadMeshes = m_chunks.find(pos) != m_chunks.end();
        m_chunks[pos]  = std::move(readyMeshes.meshes);

        ChunkFadeState &fadeState = m_chunkFadeStates[pos];
        if (!hadMeshes)
        {
            fadeState.pendingFade = true;
        }

        if (m_lightingMode == LightingMode::OLD && Lighting::isOn() && m_lastSkyLightClamp != 15 &&
            m_lastSkyLightClamp != 255)
        {
            if (m_skyQueued.insert(pos).second)
            {
                m_skyQueue.push_back(pos);
            }
        }

        it = m_readyMeshes.erase(it);
    }
}

void LevelRenderer::scheduleMesher()
{
    if (!m_mesherRunning || !m_mesherPool)
    {
        return;
    }

    bool smoothLighting   = Lighting::isOn() && m_lightingMode == LightingMode::NEW;
    bool grassSideOverlay = m_grassSideOverlayEnabled;
    int maxMesherTasks    = (int) m_maxMesherTasks;
    if (smoothLighting && maxMesherTasks > 2)
    {
        maxMesherTasks = 2;
    }

    while (m_activeMesherTasks.load() < maxMesherTasks)
    {
        ChunkPos pos;
        bool haveTask       = false;
        bool urgent         = false;
        uint64_t generation = 0;

        {
            std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);

            if (!m_urgentQueue.empty())
            {
                pos = m_urgentQueue.front();
                m_urgentQueue.pop();
                m_urgentQueued.erase(pos);
                haveTask = true;
                urgent   = true;
            }
            else if (!m_rebuildQueue.empty())
            {
                pos = m_rebuildQueue.front();
                m_rebuildQueue.pop();
                m_rebuildQueued.erase(pos);
                haveTask = true;
            }

            if (haveTask)
            {
                if (m_activeRebuilds.find(pos) != m_activeRebuilds.end())
                {
                    if (urgent)
                    {
                        m_deferredRebuilds.erase(pos);
                        m_deferredUrgentRebuilds.insert(pos);
                    }
                    else if (m_deferredUrgentRebuilds.find(pos) == m_deferredUrgentRebuilds.end())
                    {
                        m_deferredRebuilds.insert(pos);
                    }
                    haveTask = false;
                }
                else
                {
                    m_activeRebuilds.insert(pos);
                    generation = m_requestedMeshGenerations[pos];
                }
            }
        }

        if (!haveTask)
        {
            break;
        }

        const Chunk *chunk = m_level->getChunk(pos);
        if (!chunk)
        {
            std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
            m_activeRebuilds.erase(pos);
            continue;
        }

        m_activeMesherTasks.fetch_add(1);

        m_mesherPool->detachTask([this, pos, chunk, smoothLighting, grassSideOverlay, generation] {
            ThreadStorage::useDefaultThreadStorage();
            if (m_mesherRunning)
            {
                std::vector<ChunkMesher::MeshBuildResult> results;
                ChunkMesher::buildMeshes(m_level, chunk, smoothLighting, grassSideOverlay,
                                         &results);
                submitMesh(pos, generation, std::move(results));
            }

            {
                std::lock_guard<std::mutex> lock(m_rebuildQueueMutex);
                m_activeRebuilds.erase(pos);

                if (m_deferredUrgentRebuilds.erase(pos) != 0)
                {
                    if (m_urgentQueued.insert(pos).second)
                    {
                        m_urgentQueue.push(pos);
                    }
                }
                else if (m_deferredRebuilds.erase(pos) != 0)
                {
                    if (m_rebuildQueued.insert(pos).second)
                    {
                        m_rebuildQueue.push(pos);
                    }
                }
            }

            m_activeMesherTasks.fetch_sub(1);
        });
    }
}

void LevelRenderer::submitMesh(const ChunkPos &pos, uint64_t generation,
                               std::vector<ChunkMesher::MeshBuildResult> &&results)
{
    std::lock_guard<std::mutex> lock(m_meshQueueMutex);
    m_pendingMeshes.push_back({pos, generation, std::move(results)});
}
