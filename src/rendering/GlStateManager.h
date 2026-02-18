#pragma once

#include <cstdint>
#include <vector>

#include "../utils/math/Mat4.h"

class GlStateManager {
public:
    static void setClearColor(float r, float g, float b, float a);
    static void clearColor();
    static void clearDepth();
    static void clearStencil();
    static void clearAll();

    static void enableDepthTest();
    static void disableDepthTest();
    static void setDepthFunc(uint32_t func);
    static void setDepthMask(bool enabled);

    static void enableCull();
    static void disableCull();
    static void setCullFace(uint32_t face);
    static void setFrontFace(uint32_t winding);

    static void enableBlend();
    static void disableBlend();
    static void setBlendFunc(uint32_t src, uint32_t dst);

    static void enableStencilTest();
    static void disableStencilTest();
    static void setStencilMask(uint32_t mask);
    static void setStencilFunc(uint32_t func, int ref, uint32_t mask);
    static void setStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass);

    static void enableScissorTest();
    static void disableScissorTest();
    static void setScissor(int x, int y, int w, int h);

    static void enableDither();
    static void disableDither();

    static void enableMultisample();
    static void disableMultisample();

    static void enableFog();
    static void disableFog();
    static bool isFogEnabled();

    static void setFogColor(float r, float g, float b);
    static void getFogColor(float &r, float &g, float &b);

    static void setFogRange(float start, float end);
    static void getFogRange(float &start, float &end);

    static void setColorMask(bool r, bool g, bool b, bool a);

    static void setPolygonMode(uint32_t face, uint32_t mode);
    static void setLineWidth(float width);
    static void setPointSize(float size);

    static void pushMatrix();
    static void popMatrix();
    static void loadIdentity();
    static void loadMatrix(const Mat4 &mat);
    static void multMatrix(const Mat4 &mat);

    static void translatef(float x, float y, float z);
    static void scalef(float x, float y, float z);
    static void rotatef(float angleDegrees, float x, float y, float z);

    static const Mat4 &getMatrix();

private:
    static std::vector<Mat4> s_matrixStack;
    static Mat4 s_matrix;

    static bool s_depthTest;
    static bool s_depthMask;
    static uint32_t s_depthFunc;

    static bool s_cull;
    static uint32_t s_cullFace;
    static uint32_t s_frontFace;

    static bool s_blend;
    static uint32_t s_blendSrc;
    static uint32_t s_blendDst;

    static bool s_stencil;
    static uint32_t s_stencilMask;

    static bool s_scissor;
    static bool s_dither;
    static bool s_multisample;

    static bool s_fog;
    static float s_fogR;
    static float s_fogG;
    static float s_fogB;
    static float s_fogStart;
    static float s_fogEnd;

    static bool s_colorMaskR;
    static bool s_colorMaskG;
    static bool s_colorMaskB;
    static bool s_colorMaskA;

    static uint32_t s_polygonFace;
    static uint32_t s_polygonMode;

    static float s_lineWidth;
    static float s_pointSize;
};
