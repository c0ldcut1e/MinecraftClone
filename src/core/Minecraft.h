#pragma once

#include "../entity/LocalPlayer.h"
#include "../graphics/Shader.h"
#include "../scene/Camera.h"
#include "../utils/math/Mat4.h"
#include "../world/World.h"
#include "../world/chunk/ChunkManager.h"
#include "Window.h"

class WorldRenderer;

class Minecraft {
public:
    static Minecraft *getInstance();

    void start();

    const Camera *getCamera() const;
    LocalPlayer *getLocalPlayer() const;
    WorldRenderer *getWorldRenderer() const;
    const Mat4 &getProjection() const;
    ChunkManager *getChunkManager() const;

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

    Shader *m_shader;

    Camera *m_camera;
    World *m_world;

    LocalPlayer *m_player;

    WorldRenderer *m_worldRenderer;
    ChunkManager *m_chunkManager;

    double m_farPlane;
    Mat4 m_projection;
    Mat4 m_model;

    bool m_mouseLocked;
};
