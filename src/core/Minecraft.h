#pragma once

#include "../entity/LocalPlayer.h"
#include "../rendering/Font.h"
#include "../rendering/Shader.h"
#include "../scene/Camera.h"
#include "../ui/ImGuiSystem.h"
#include "../ui/UIController.h"
#include "../utils/Timer.h"
#include "../utils/math/Mat4.h"
#include "../world/Level.h"
#include "../world/chunk/ChunkManager.h"
#include "Window.h"

class LevelRenderer;
class UIScene_DebugOverlay;

class Minecraft
{
public:
    static Minecraft *getInstance();

    void start();
    void shutdown();

    int getWidth() const;
    int getHeight() const;

    Timer *getTimer() const;

    const Camera *getCamera() const;

    LocalPlayer *getLocalPlayer() const;
    Level *getLevel() const;

    LevelRenderer *getLevelRenderer() const;

    const Mat4 &getProjection() const;

    ChunkManager *getChunkManager() const;

    Font *getDefaultFont() const;

private:
    Minecraft();
    ~Minecraft();

    Minecraft(const Minecraft &)            = delete;
    Minecraft &operator=(const Minecraft &) = delete;

    Minecraft(Minecraft &&)            = delete;
    Minecraft &operator=(Minecraft &&) = delete;

    void renderFrame();
    void toggleMouseLock();
    void toggleImGuiOverlay();
    void toggleDebugOverlay();

    void initRegistries();

    int m_width;
    int m_height;
    Window m_window;

    bool m_shutdown;

    Timer *m_timer;

    Camera *m_camera;
    Level *m_level;

    LocalPlayer *m_localPlayer;

    LevelRenderer *m_levelRenderer;
    ChunkManager *m_chunkManager;

    Font *m_defaultFont;
    UIController *m_uiController;
    ImGuiSystem *m_imguiSystem;
    UIScene_DebugOverlay *m_debugOverlayScene;

    double m_farPlane;
    Mat4 m_projection;
    Vec3 m_playerSpawnPosition;

    bool m_mouseLocked;
    bool m_restoreMouseLockAfterImGui;
};
