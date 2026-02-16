#pragma once

#include "../entity/LocalPlayer.h"
#include "../rendering/Font.h"
#include "../rendering/Shader.h"
#include "../scene/Camera.h"
#include "../ui/UIController.h"
#include "../utils/FixedStepper.h"
#include "../utils/math/Mat4.h"
#include "../world/World.h"
#include "../world/chunk/ChunkManager.h"
#include "Window.h"

class WorldRenderer;

class Minecraft {
public:
    static Minecraft *getInstance();

    void start();
    void shutdown();

    int getWidth() const;
    int getHeight() const;

    FixedStepper *getFixedStepper() const;

    const Camera *getCamera() const;

    LocalPlayer *getLocalPlayer() const;

    WorldRenderer *getWorldRenderer() const;

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

    void initRegistries();

    int m_width;
    int m_height;
    Window m_window;

    bool m_shutdown;

    FixedStepper *m_fixedStepper;

    Camera *m_camera;
    World *m_world;

    LocalPlayer *m_localPlayer;

    WorldRenderer *m_worldRenderer;
    ChunkManager *m_chunkManager;

    Font *m_defaultFont;
    UIController *m_uiController;

    double m_farPlane;
    Mat4 m_projection;

    bool m_mouseLocked;
};
