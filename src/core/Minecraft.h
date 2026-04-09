#pragma once

#include "../entity/LocalPlayer.h"
#include "../rendering/Font.h"
#include "../rendering/Framebuffer.h"
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
class InputManager;
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
    InputManager *getInputManager() const;
    float getGammaPercent() const;
    void setGammaPercent(float gammaPercent);

private:
    Minecraft();
    ~Minecraft();

    Minecraft(const Minecraft &)            = delete;
    Minecraft &operator=(const Minecraft &) = delete;

    Minecraft(Minecraft &&)            = delete;
    Minecraft &operator=(Minecraft &&) = delete;

    void renderFrame();
    void setMouseLock(bool locked);
    void toggleMouseLock();
    void toggleImGuiOverlay();
    void toggleDebugOverlay();
    void renderGammaPass();

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
    Framebuffer *m_frameFramebuffer;
    Shader *m_gammaShader;
    uint32_t m_gammaFullscreenVao;
    UIController *m_uiController;
    ImGuiSystem *m_imguiSystem;
    InputManager *m_inputManager;
    UIScene_DebugOverlay *m_debugOverlayScene;

    double m_farPlane;
    Mat4 m_projection;
    Vec3 m_playerSpawnPosition;
    float m_gammaPercent;

    bool m_inWorld;
    bool m_mouseLocked;
    bool m_restoreMouseLockAfterImGui;
};
