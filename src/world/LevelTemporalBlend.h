#pragma once

#include "../rendering/Framebuffer.h"
#include "../rendering/Shader.h"
#include "../utils/math/Vec3.h"

class LevelTemporalBlend
{
public:
    struct Settings
    {
        float historyWeight;
        float motionFadeDistance;
        float depthTolerance;
        float depthFadeRange;
    };

    LevelTemporalBlend(int width, int height);
    ~LevelTemporalBlend();

    void resize(int width, int height);
    void reset();

    Framebuffer *beginWrite();
    void compositeToScene(const Framebuffer &sceneFramebuffer, const Vec3 &cameraPos);
    const Framebuffer *getCurrentTarget() const;

    Settings &getSettings();
    const Settings &getSettings() const;

private:
    float getHistoryWeight(const Vec3 &cameraPos) const;
    void copyDepthToScene(const Framebuffer &source, const Framebuffer &sceneFramebuffer) const;

    Shader *m_shader;
    Framebuffer *m_targets[2];
    uint32_t m_fullscreenVao;
    int m_writeIndex;
    int m_historyIndex;
    bool m_hasHistory;
    Vec3 m_previousCameraPos;
    Settings m_settings;
};
