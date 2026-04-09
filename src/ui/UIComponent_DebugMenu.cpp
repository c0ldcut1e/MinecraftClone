#include "UIComponent_DebugMenu.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <unistd.h>
#include <vector>

#include <glad/glad.h>

#include "../core/Minecraft.h"
#include "../memory/MemoryTracker.h"
#include "../rendering/Font.h"
#include "../rendering/GlStateManager.h"
#include "../rendering/Tesselator.h"
#include "../utils/Time.h"
#include "../utils/math/Mth.h"
#include "../world/Level.h"
#include "../world/LevelRenderer.h"
#include "../world/block/Block.h"
#include "../world/chunk/Chunk.h"
#include "../world/chunk/ChunkManager.h"
#include "../world/chunk/ChunkPos.h"
#include "UIScreen.h"

#ifndef GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#endif

#ifndef GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
#endif

#ifndef GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#endif

#ifndef GL_TEXTURE_FREE_MEMORY_ATI
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
#endif

struct VramStats
{
    bool available;
    bool hasTotal;
    uint64_t totalBytes;
    uint64_t freeBytes;
    std::wstring label;
};

static std::wstring widen(const char *text)
{
    if (!text)
    {
        return L"(null)";
    }

    std::string value(text);
    return std::wstring(value.begin(), value.end());
}

static std::wstring formatBytes(uint64_t bytes)
{
    wchar_t buffer[64];

    if (bytes >= 1024ull * 1024ull * 1024ull)
    {
        swprintf(buffer, 64, L"%.2f GiB", (double) bytes / (1024.0 * 1024.0 * 1024.0));
        return buffer;
    }

    if (bytes >= 1024ull * 1024ull)
    {
        swprintf(buffer, 64, L"%.2f MiB", (double) bytes / (1024.0 * 1024.0));
        return buffer;
    }

    if (bytes >= 1024ull)
    {
        swprintf(buffer, 64, L"%.2f KiB", (double) bytes / 1024.0);
        return buffer;
    }

    swprintf(buffer, 64, L"%llu B", (unsigned long long) bytes);
    return buffer;
}

static bool hasGlExtension(const char *name)
{
    if (!name || !*name)
    {
        return false;
    }

    if (glGetStringi)
    {
        GLint extensionCount = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
        for (GLint i = 0; i < extensionCount; i++)
        {
            const char *extension = (const char *) glGetStringi(GL_EXTENSIONS, (GLuint) i);
            if (extension && strcmp(extension, name) == 0)
            {
                return true;
            }
        }
        return false;
    }

    const char *extensions = (const char *) glGetString(GL_EXTENSIONS);
    if (!extensions)
    {
        return false;
    }

    const char *match = strstr(extensions, name);
    while (match)
    {
        const char *end = match + strlen(name);
        bool startOk    = match == extensions || *(match - 1) == ' ';
        bool endOk      = *end == '\0' || *end == ' ';
        if (startOk && endOk)
        {
            return true;
        }
        match = strstr(end, name);
    }

    return false;
}

static bool queryProcessResidentBytes(uint64_t *outBytes)
{
    if (!outBytes)
    {
        return false;
    }

#if defined(__linux__)
    FILE *file = fopen("/proc/self/statm", "r");
    if (!file)
    {
        return false;
    }

    unsigned long totalPages    = 0;
    unsigned long residentPages = 0;
    int fields                  = fscanf(file, "%lu %lu", &totalPages, &residentPages);
    fclose(file);
    if (fields != 2)
    {
        return false;
    }

    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
    {
        return false;
    }

    *outBytes = (uint64_t) residentPages * (uint64_t) pageSize;
    return true;
#else
    (void) outBytes;
    return false;
#endif
}

static VramStats queryVramStats()
{
    VramStats stats;
    stats.available  = false;
    stats.hasTotal   = false;
    stats.totalBytes = 0;
    stats.freeBytes  = 0;
    stats.label      = L"";

    if (hasGlExtension("GL_NVX_gpu_memory_info"))
    {
        GLint dedicatedKb = 0;
        GLint totalKb     = 0;
        GLint currentKb   = 0;

        glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicatedKb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalKb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currentKb);

        if (dedicatedKb > 0 || totalKb > 0 || currentKb > 0)
        {
            stats.available  = true;
            stats.freeBytes  = currentKb > 0 ? (uint64_t) currentKb * 1024ull : 0;
            stats.totalBytes = dedicatedKb > 0 ? (uint64_t) dedicatedKb * 1024ull
                                               : (uint64_t) totalKb * 1024ull;
            stats.hasTotal   = stats.totalBytes > 0;
            stats.label      = L"NVX";
        }

        return stats;
    }

    if (hasGlExtension("GL_ATI_meminfo"))
    {
        GLint textureFreeKb[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, textureFreeKb);

        if (textureFreeKb[0] > 0)
        {
            stats.available  = true;
            stats.hasTotal   = false;
            stats.freeBytes  = (uint64_t) textureFreeKb[0] * 1024ull;
            stats.totalBytes = 0;
            stats.label      = L"ATI";
        }
    }

    return stats;
}

UIComponent_DebugMenu::UIComponent_DebugMenu()
    : UIComponent("ComponentDebugPanel"), m_statsRefreshTimer(0.0), m_statsRefreshInterval(0.25)
{}

UIComponent_DebugMenu::~UIComponent_DebugMenu() {}

void UIComponent_DebugMenu::tick()
{
    m_statsRefreshTimer += Time::getDelta();
    if (m_statsRefreshTimer >= m_statsRefreshInterval)
    {
        refreshLines();
        m_statsRefreshTimer = 0.0;
    }
}

void UIComponent_DebugMenu::refreshLines()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        m_cachedLines.clear();
        return;
    }

    LocalPlayer *me = minecraft->getLocalPlayer();
    if (!me)
    {
        m_cachedLines.clear();
        return;
    }

    Level *level = me->getLevel();

    std::vector<std::wstring> lines;

    Timer *timer = minecraft->getTimer();
    double delta = timer ? (double) timer->getFrameSeconds() : Time::getDelta();
    wchar_t buffer[0x1FF];
    swprintf(buffer, 0xFF, L"dt: %.3f ms  fps: %.1f", delta * 1000.0,
             delta > 0.0 ? 1.0 / delta : 0.0);
    lines.emplace_back(buffer);

    swprintf(buffer, 0x1FF, L"ticks: %d  partial: %.3f  tick dt: %.3f ms",
             timer ? timer->getElapsedTicks() : 0, timer ? timer->getPartialTicks() : 0.0f,
             Time::getTickDelta() * 1000.0);
    lines.emplace_back(buffer);

    swprintf(buffer, 0x1FF, L"display: %d x %d", minecraft->getWidth(), minecraft->getHeight());
    lines.emplace_back(buffer);

    MemoryTracker::Stats memoryStats = MemoryTracker::getInstance()->getStats();
    uint64_t residentBytes           = 0;
    bool hasResidentBytes            = queryProcessResidentBytes(&residentBytes);
    std::wstring trackedBytes        = formatBytes(memoryStats.trackedAllocatedBytes);
    std::wstring residentLabel =
            hasResidentBytes ? formatBytes(residentBytes) : std::wstring(L"(n/a)");

    swprintf(buffer, 0x1FF, L"memory: rss %ls  tracked %ls", residentLabel.c_str(),
             trackedBytes.c_str());
    lines.emplace_back(buffer);

    swprintf(buffer, 0x1FF, L"gl objs: tex %d  buf %d  vao %d  list %d", memoryStats.glTextureCount,
             memoryStats.glBufferCount, memoryStats.glVertexArrayCount, memoryStats.glListCount);
    lines.emplace_back(buffer);

    VramStats vramStats = queryVramStats();
    if (vramStats.available)
    {
        std::wstring freeBytes = formatBytes(vramStats.freeBytes);
        if (vramStats.hasTotal)
        {
            std::wstring totalBytes = formatBytes(vramStats.totalBytes);
            swprintf(buffer, 0x1FF, L"vram (%ls): free %ls / total %ls", vramStats.label.c_str(),
                     freeBytes.c_str(), totalBytes.c_str());
        }
        else
        {
            swprintf(buffer, 0x1FF, L"vram (%ls): free %ls", vramStats.label.c_str(),
                     freeBytes.c_str());
        }
    }
    else
    {
        swprintf(buffer, 0x1FF, L"vram: unavailable");
    }
    lines.emplace_back(buffer);

    lines.emplace_back(L"");

    std::wstring glVendor   = widen((const char *) glGetString(GL_VENDOR));
    std::wstring glRenderer = widen((const char *) glGetString(GL_RENDERER));
    std::wstring glVersion  = widen((const char *) glGetString(GL_VERSION));
    swprintf(buffer, 0x1FF, L"gpu vendor: %ls", glVendor.c_str());
    lines.emplace_back(buffer);
    swprintf(buffer, 0x1FF, L"gpu renderer: %ls", glRenderer.c_str());
    lines.emplace_back(buffer);
    swprintf(buffer, 0x1FF, L"gpu version: %ls", glVersion.c_str());
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

        if (level)
        {
            const Chunk *playerChunk = level->getChunk(ChunkPos(cx, cy, cz));
            int lx                   = Mth::floorMod((int) floor(pos.x), Chunk::SIZE_X);
            int ly                   = Mth::floorMod((int) floor(pos.y), Chunk::SIZE_Y);
            int lz                   = Mth::floorMod((int) floor(pos.z), Chunk::SIZE_Z);

            if (playerChunk)
            {
                uint8_t lightR = 0;
                uint8_t lightG = 0;
                uint8_t lightB = 0;
                uint8_t sky    = 0;
                playerChunk->getBlockLight(lx, ly, lz, &lightR, &lightG, &lightB);
                sky = playerChunk->getSkyLight(lx, ly, lz);

                swprintf(buffer, 0x1FF, L"player light: block %u/%u/%u  sky %u", lightR, lightG,
                         lightB, sky);
                lines.emplace_back(buffer);
            }
        }

        if (level)
        {
            LevelRenderer *levelRenderer = minecraft->getLevelRenderer();
            ChunkManager *chunkManager   = minecraft->getChunkManager();
            uint32_t loadedChunks        = (uint32_t) level->getChunks().size();
            uint32_t meshedChunks =
                    levelRenderer ? (uint32_t) levelRenderer->getMeshedChunkCount() : 0;
            uint32_t visibleChunks =
                    levelRenderer ? (uint32_t) levelRenderer->getVisibleChunkCount() : 0;
            uint32_t renderedMeshes =
                    levelRenderer ? (uint32_t) levelRenderer->getRenderedMeshCount() : 0;

            swprintf(buffer, 0xFF, L"chunks: loaded %u  meshed %u  visible %u", loadedChunks,
                     meshedChunks, visibleChunks);
            lines.emplace_back(buffer);

            swprintf(buffer, 0xFF, L"render: meshes %u  distance %d", renderedMeshes,
                     level->getRenderDistance());
            lines.emplace_back(buffer);

            if (levelRenderer)
            {
                const wchar_t *mode =
                        levelRenderer->getLightingMode() == LevelRenderer::LightingMode::NEW
                                ? L"new"
                                : (levelRenderer->getLightingMode() ==
                                                   LevelRenderer::LightingMode::OLD
                                           ? L"old"
                                           : L"off");
                swprintf(buffer, 0xFF, L"lighting: %ls  (L to toggle)", mode);
                lines.emplace_back(buffer);

                const wchar_t *blockOutline =
                        levelRenderer->getBlockOutlineMode() ==
                                        LevelRenderer::BlockOutlineMode::BEDROCK
                                ? L"bedrock"
                                : L"normal";
                swprintf(buffer, 0xFF, L"block outline: %ls  (B to toggle)", blockOutline);
                lines.emplace_back(buffer);

                const wchar_t *grassOverlay =
                        levelRenderer->isGrassSideOverlayEnabled() ? L"on" : L"off";
                swprintf(buffer, 0xFF, L"grass side overlay: %ls  (G to toggle)", grassOverlay);
                lines.emplace_back(buffer);

                swprintf(buffer, 0xFF, L"mesher: upload %u  rebuild %u  urgent %u  sky %u",
                         (uint32_t) levelRenderer->getPendingMeshCount(),
                         (uint32_t) levelRenderer->getQueuedRebuildCount(),
                         (uint32_t) levelRenderer->getUrgentRebuildCount(),
                         (uint32_t) levelRenderer->getSkyQueueCount());
                lines.emplace_back(buffer);

                swprintf(buffer, 0xFF, L"mesher threads: %u",
                         (uint32_t) levelRenderer->getMesherThreadCount());
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

            swprintf(buffer, 0xFF, L"level q: dirty %u  urgent %u  light %u  entities %u",
                     (uint32_t) level->getQueuedDirtyChunkCount(),
                     (uint32_t) level->getUrgentDirtyChunkCount(),
                     (uint32_t) level->getQueuedLightUpdateCount(),
                     (uint32_t) level->getEntities().size());
            lines.emplace_back(buffer);

            lines.emplace_back(L"");

            HitResult *result = level->clip(pos.add(Vec3(0.0, 1.6, 0.0)), front, 8.0f);
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

                    const Chunk *chunk = level->getChunk({bcx, bcy, bcz});
                    uint32_t blockId   = 0;
                    Block *block       = nullptr;

                    if (chunk && ly >= 0 && ly < Chunk::SIZE_Y)
                    {
                        uint8_t lightR = 0;
                        uint8_t lightG = 0;
                        uint8_t lightB = 0;
                        uint8_t sky    = 0;
                        blockId        = chunk->getBlockId(lx, ly, lz);
                        block          = Block::byId(blockId);
                        chunk->getBlockLight(lx, ly, lz, &lightR, &lightG, &lightB);
                        sky = chunk->getSkyLight(lx, ly, lz);

                        swprintf(buffer, 0x1FF, L"target light: block %u/%u/%u  sky %u", lightR,
                                 lightG, lightB, sky);
                        lines.emplace_back(buffer);
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

            swprintf(buffer, 0xFF, L"dimension time: %llu", level->getDimensionTime().getTicks());
            lines.emplace_back(buffer);
        }
    }

    m_cachedLines = std::move(lines);
}

void UIComponent_DebugMenu::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    if (m_cachedLines.empty())
    {
        refreshLines();
        if (m_cachedLines.empty())
        {
            return;
        }
    }

    Font *font      = minecraft->getDefaultFont();
    float uiScale   = UIScreen::scaleUniform(minecraft->getWidth(), minecraft->getHeight());
    float fontScale = font ? font->snapScale(uiScale) : uiScale;

    float x0 = UIScreen::toActualX(20.0f, minecraft->getWidth());
    float y0 = UIScreen::toActualY(20.0f, minecraft->getWidth(), minecraft->getHeight());

    float padX       = 10.0f * uiScale;
    float padY       = 8.0f * uiScale;
    float lineHeight = font ? font->getLineHeight(fontScale) : 24.0f * uiScale;

    float maxWidth = 0.0f;
    if (font)
    {
        for (const std::wstring &line : m_cachedLines)
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

        for (uint32_t i = 0; i < (uint32_t) m_cachedLines.size(); i++)
        {
            font->drawShadow(m_cachedLines[i], tx, ty + (float) i * lineHeight, fontScale, -1);
        }
    }
}
