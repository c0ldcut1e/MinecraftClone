#include "UIComponent_DebugPanel.h"

#include <cmath>
#include <cwchar>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../rendering/Font.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/Tesselator.h"
#include "../utils/Time.h"
#include "../utils/math/Mth.h"
#include "../world/World.h"
#include "../world/WorldRenderer.h"
#include "../world/block/Block.h"
#include "../world/chunk/Chunk.h"
#include "../world/chunk/ChunkManager.h"
#include "../world/chunk/ChunkPos.h"

UIComponent_DebugPanel::UIComponent_DebugPanel() : UIComponent("ComponentDebugPanel") {}

UIComponent_DebugPanel::~UIComponent_DebugPanel() {}

void UIComponent_DebugPanel::tick() {}

void UIComponent_DebugPanel::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    LocalPlayer *me      = minecraft->getLocalPlayer();
    World *world         = me->getWorld();

    std::vector<std::wstring> lines;

    double delta = Time::getDelta();
    wchar_t buffer[0xFF];
    swprintf(buffer, 0xFF, L"dt: %.3f ms  fps: %.1f", delta * 1000.0,
             delta > 0.0 ? 1.0 / delta : 0.0);
    lines.emplace_back(buffer);

    lines.emplace_back(L"");

    if (me)
    {
        const Vec3 &pos = me->getPosition();
        swprintf(buffer, 0xFF, L"pos: %.3f / %.3f / %.3f", (float) pos.x, (float) pos.y,
                 (float) pos.z);
        lines.emplace_back(buffer);

        swprintf(buffer, 0xFF, L"rot: yaw %.2f  pitch %.2f", me->getYaw(), me->getPitch());
        lines.emplace_back(buffer);

        const Vec3 &front = me->getFront();
        swprintf(buffer, 0xFF, L"facing: %.3f / %.3f / %.3f", (float) front.x, (float) front.y,
                 (float) front.z);
        lines.emplace_back(buffer);

        int cx = Mth::floorDiv((int) pos.x, Chunk::SIZE_X);
        int cy = Mth::floorDiv((int) pos.y, Chunk::SIZE_Y);
        int cz = Mth::floorDiv((int) pos.z, Chunk::SIZE_Z);

        lines.emplace_back(L"");

        swprintf(buffer, 0xFF, L"chunk: %d / %d / %d", cx, cy, cz);
        lines.emplace_back(buffer);

        if (world)
        {
            WorldRenderer *worldRenderer = minecraft->getWorldRenderer();
            ChunkManager *chunkManager   = minecraft->getChunkManager();

            uint32_t loadedChunks  = (uint32_t) world->getChunks().size();
            uint32_t meshedChunks  = worldRenderer ? (uint32_t) worldRenderer->getMeshedChunkCount() : 0;
            uint32_t visibleChunks = worldRenderer ? (uint32_t) worldRenderer->getVisibleChunkCount() : 0;
            uint32_t renderedMeshes =
                    worldRenderer ? (uint32_t) worldRenderer->getRenderedMeshCount() : 0;

            swprintf(buffer, 0xFF, L"chunks: loaded %u  meshed %u  visible %u", loadedChunks,
                     meshedChunks, visibleChunks);
            lines.emplace_back(buffer);

            swprintf(buffer, 0xFF, L"render: meshes %u  distance %d", renderedMeshes,
                     world->getRenderDistance());
            lines.emplace_back(buffer);

            if (worldRenderer)
            {
                const wchar_t *mode = worldRenderer->getLightingMode() ==
                                              WorldRenderer::LightingMode::NEW
                                              ? L"new"
                                              : L"old";
                swprintf(buffer, 0xFF, L"lighting: %ls  (L to toggle)", mode);
                lines.emplace_back(buffer);

                const wchar_t *grassOverlay =
                        worldRenderer->isGrassSideOverlayEnabled() ? L"on" : L"off";
                swprintf(buffer, 0xFF, L"grass side overlay: %ls  (G to toggle)", grassOverlay);
                lines.emplace_back(buffer);

                swprintf(buffer, 0xFF, L"mesher: upload %u  rebuild %u  urgent %u  sky %u",
                         (uint32_t) worldRenderer->getPendingMeshCount(),
                         (uint32_t) worldRenderer->getQueuedRebuildCount(),
                         (uint32_t) worldRenderer->getUrgentRebuildCount(),
                         (uint32_t) worldRenderer->getSkyQueueCount());
                lines.emplace_back(buffer);

                swprintf(buffer, 0xFF, L"mesher threads: %u",
                         (uint32_t) worldRenderer->getMesherThreadCount());
                lines.emplace_back(buffer);
            }

            if (chunkManager)
            {
                swprintf(buffer, 0xFF, L"generator: pending %u  active %u/%u  ready %u  threads %u",
                         (uint32_t) chunkManager->getPendingCount(),
                         (uint32_t) chunkManager->getActiveCount(),
                         (uint32_t) chunkManager->getMaxActiveCount(),
                         (uint32_t) chunkManager->getFinishedCount(),
                         (uint32_t) chunkManager->getThreadCount());
                lines.emplace_back(buffer);
            }

            swprintf(buffer, 0xFF, L"world q: dirty %u  urgent %u  light %u  entities %u",
                     (uint32_t) world->getQueuedDirtyChunkCount(),
                     (uint32_t) world->getUrgentDirtyChunkCount(),
                     (uint32_t) world->getQueuedLightUpdateCount(),
                     (uint32_t) world->getEntities().size());
            lines.emplace_back(buffer);

            lines.emplace_back(L"");

            HitResult *result = world->clip(pos.add(Vec3(0.0, 1.6, 0.0)), front, 8.0f);
            if (result)
            {
                if (result->isBlock())
                {
                    BlockPos hit       = result->getBlockPos();
                    Direction *hitFace = result->getBlockFace();

                    int bcx = Mth::floorDiv(hit.x, Chunk::SIZE_X);
                    int bcy = Mth::floorDiv(hit.y, Chunk::SIZE_Y);
                    int bcz = Mth::floorDiv(hit.z, Chunk::SIZE_Z);

                    int lx = Mth::floorMod(hit.x, Chunk::SIZE_X);
                    int ly = hit.y;
                    int lz = Mth::floorMod(hit.z, Chunk::SIZE_Z);

                    const Chunk *chunk = world->getChunk({bcx, bcy, bcz});
                    uint32_t blockId   = 0;
                    Block *block       = nullptr;

                    if (chunk && ly >= 0 && ly < Chunk::SIZE_Y)
                    {
                        blockId = chunk->getBlockId(lx, ly, lz);
                        block   = Block::byId(blockId);
                    }

                    std::wstring blockName;
                    if (block)
                    {
                        const std::string &name = block->getName();
                        blockName               = std::wstring(name.begin(), name.end());
                    }
                    else
                    {
                        blockName = L"(null)";
                    }

                    swprintf(buffer, 0xFF, L"target: %d %d %d  %s", hit.x, hit.y, hit.z,
                             hitFace->name.c_str());
                    lines.emplace_back(buffer);

                    swprintf(buffer, 0xFF, L"block: %d  %ls", blockId, blockName.c_str());
                    lines.emplace_back(buffer);
                }
                else if (result->isEntity())
                {
                    swprintf(buffer, 0xFF, L"target entity: type 0x%016llx",
                             result->getEntityType());
                    lines.emplace_back(buffer);
                }
                else
                {
                    swprintf(buffer, 0xFF, L"target: (none)");
                    lines.emplace_back(buffer);
                }

                delete result;
            }

            lines.emplace_back(L"");

            swprintf(buffer, 0xFF, L"worldTime: %llu", world->getWorldTime().getTicks());
            lines.emplace_back(buffer);
        }

        Font *font      = minecraft->getDefaultFont();
        float fontScale = 1.3f;

        float x0 = 20.0f;
        float y0 = 20.0f;

        float padX       = 10.0f;
        float padY       = 8.0f;
        float lineHeight = 24.0f;

        float maxWidth = 0.0f;
        if (font)
        {
            for (const std::wstring &line : lines)
            {
                float width = font->getWidth(line, fontScale);
                if (width > maxWidth)
                    maxWidth = width;
            }
        }
        else
        {
            maxWidth = 360.0f;
        }

        if (font)
        {
            float tx = x0 + padX;
            float ty = y0 + padY + 2.0f;

            for (uint32_t i = 0; i < (uint32_t) lines.size(); i++)
            {
                font->renderShadow(lines[i], tx, ty + (float) i * lineHeight, fontScale, -1);
            }
        }
    }
}
