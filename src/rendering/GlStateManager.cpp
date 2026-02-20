#include "GlStateManager.h"

#include <cmath>

#include <glad/glad.h>

#include "RenderCommand.h"

std::vector<Mat4> GlStateManager::s_matrixStack;
Mat4 GlStateManager::s_matrix = Mat4::identity();

bool GlStateManager::s_depthTest     = true;
bool GlStateManager::s_depthMask     = true;
uint32_t GlStateManager::s_depthFunc = GL_LESS;

bool GlStateManager::s_cull          = true;
uint32_t GlStateManager::s_cullFace  = GL_BACK;
uint32_t GlStateManager::s_frontFace = GL_CCW;

bool GlStateManager::s_blend        = false;
uint32_t GlStateManager::s_blendSrc = GL_SRC_ALPHA;
uint32_t GlStateManager::s_blendDst = GL_ONE_MINUS_SRC_ALPHA;

bool GlStateManager::s_stencil         = false;
uint32_t GlStateManager::s_stencilMask = 0xFFFFFFFF;

bool GlStateManager::s_scissor     = false;
bool GlStateManager::s_dither      = false;
bool GlStateManager::s_multisample = true;

bool GlStateManager::s_fog       = true;
float GlStateManager::s_fogR     = 0.47058824f;
float GlStateManager::s_fogG     = 0.65490198f;
float GlStateManager::s_fogB     = 1.0f;
float GlStateManager::s_fogStart = 0.0f;
float GlStateManager::s_fogEnd   = 0.0f;

bool GlStateManager::s_colorMaskR = true;
bool GlStateManager::s_colorMaskG = true;
bool GlStateManager::s_colorMaskB = true;
bool GlStateManager::s_colorMaskA = true;

bool GlStateManager::s_polygonOffsetFill    = false;
float GlStateManager::s_polygonOffsetFactor = 0.0f;
float GlStateManager::s_polygonOffsetUnits  = 0.0f;
uint32_t GlStateManager::s_polygonFace      = GL_FRONT_AND_BACK;
uint32_t GlStateManager::s_polygonMode      = GL_FILL;

float GlStateManager::s_lineWidth = 1.0f;
float GlStateManager::s_pointSize = 1.0f;

static inline Mat4 gsmTranslation(double x, double y, double z) {
    Mat4 matrix        = Mat4::identity();
    double *matrixData = (double *) matrix.data;
    matrixData[12]     = x;
    matrixData[13]     = y;
    matrixData[14]     = z;
    return matrix;
}

static inline Mat4 gsmScale(double x, double y, double z) {
    Mat4 matrix        = Mat4::identity();
    double *matrixData = (double *) matrix.data;
    matrixData[0]      = x;
    matrixData[5]      = y;
    matrixData[10]     = z;
    return matrix;
}

static inline Mat4 gsmRotation(double angleRadians, double x, double y, double z) {
    double len = sqrt(x * x + y * y + z * z);
    if (len == 0.0) return Mat4::identity();

    x /= len;
    y /= len;
    z /= len;

    double c = cos(angleRadians);
    double s = sin(angleRadians);
    double t = 1.0 - c;

    Mat4 matrix        = Mat4::identity();
    double *matrixData = (double *) matrix.data;

    matrixData[0] = t * x * x + c;
    matrixData[1] = t * x * y + s * z;
    matrixData[2] = t * x * z - s * y;

    matrixData[4] = t * x * y - s * z;
    matrixData[5] = t * y * y + c;
    matrixData[6] = t * y * z + s * x;

    matrixData[8]  = t * x * z + s * y;
    matrixData[9]  = t * y * z - s * x;
    matrixData[10] = t * z * z + c;

    return matrix;
}

void GlStateManager::setClearColor(float r, float g, float b, float a) { RenderCommand::setClearColor(r, g, b, a); }

void GlStateManager::clearColor() { RenderCommand::clearColor(); }

void GlStateManager::clearDepth() { RenderCommand::clearDepth(); }

void GlStateManager::clearStencil() { RenderCommand::clearStencil(); }

void GlStateManager::clearAll() { RenderCommand::clearAll(); }

void GlStateManager::enableDepthTest() {
    if (s_depthTest) return;
    s_depthTest = true;
    RenderCommand::enableDepthTest();
}

void GlStateManager::disableDepthTest() {
    if (!s_depthTest) return;
    s_depthTest = false;
    RenderCommand::disableDepthTest();
}

void GlStateManager::setDepthFunc(uint32_t func) {
    if (s_depthFunc == func) return;
    s_depthFunc = func;
    RenderCommand::setDepthFunc(func);
}

void GlStateManager::setDepthMask(bool enabled) {
    if (s_depthMask == enabled) return;
    s_depthMask = enabled;
    RenderCommand::setDepthMask(enabled);
}

void GlStateManager::enableCull() {
    if (s_cull) return;
    s_cull = true;
    RenderCommand::enableCull();
}

void GlStateManager::disableCull() {
    if (!s_cull) return;
    s_cull = false;
    RenderCommand::disableCull();
}

void GlStateManager::setCullFace(uint32_t face) {
    if (s_cullFace == face) return;
    s_cullFace = face;
    RenderCommand::setCullFace(face);
}

void GlStateManager::setFrontFace(uint32_t winding) {
    if (s_frontFace == winding) return;
    s_frontFace = winding;
    RenderCommand::setFrontFace(winding);
}

void GlStateManager::enableBlend() {
    if (s_blend) return;
    s_blend = true;
    RenderCommand::enableBlend();
}

void GlStateManager::disableBlend() {
    if (!s_blend) return;
    s_blend = false;
    RenderCommand::disableBlend();
}

void GlStateManager::setBlendFunc(uint32_t src, uint32_t dst) {
    if (s_blendSrc == src && s_blendDst == dst) return;
    s_blendSrc = src;
    s_blendDst = dst;
    RenderCommand::setBlendFunc(src, dst);
}

void GlStateManager::enableStencilTest() {
    if (s_stencil) return;
    s_stencil = true;
    RenderCommand::enableStencilTest();
}

void GlStateManager::disableStencilTest() {
    if (!s_stencil) return;
    s_stencil = false;
    RenderCommand::disableStencilTest();
}

void GlStateManager::setStencilMask(uint32_t mask) {
    if (s_stencilMask == mask) return;
    s_stencilMask = mask;
    RenderCommand::setStencilMask(mask);
}

void GlStateManager::setStencilFunc(uint32_t func, int ref, uint32_t mask) { RenderCommand::setStencilFunc(func, ref, mask); }

void GlStateManager::setStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass) { RenderCommand::setStencilOp(sfail, dpfail, dppass); }

void GlStateManager::enableScissorTest() {
    if (s_scissor) return;
    s_scissor = true;
    RenderCommand::enableScissorTest();
}

void GlStateManager::disableScissorTest() {
    if (!s_scissor) return;
    s_scissor = false;
    RenderCommand::disableScissorTest();
}

void GlStateManager::setScissor(int x, int y, int w, int h) { RenderCommand::setScissor(x, y, w, h); }

void GlStateManager::enableDither() {
    if (s_dither) return;
    s_dither = true;
    RenderCommand::enableDither();
}

void GlStateManager::disableDither() {
    if (!s_dither) return;
    s_dither = false;
    RenderCommand::disableDither();
}

void GlStateManager::enableMultisample() {
    if (s_multisample) return;
    s_multisample = true;
    RenderCommand::enableMultisample();
}

void GlStateManager::disableMultisample() {
    if (!s_multisample) return;
    s_multisample = false;
    RenderCommand::disableMultisample();
}

void GlStateManager::enableFog() { s_fog = true; }

void GlStateManager::disableFog() { s_fog = false; }

bool GlStateManager::isFogEnabled() { return s_fog; }

void GlStateManager::setFogColor(float r, float g, float b) {
    s_fogR = r;
    s_fogG = g;
    s_fogB = b;
}

void GlStateManager::getFogColor(float &r, float &g, float &b) {
    r = s_fogR;
    g = s_fogG;
    b = s_fogB;
}

void GlStateManager::setFogRange(float start, float end) {
    s_fogStart = start;
    s_fogEnd   = end;
}

void GlStateManager::getFogRange(float &start, float &end) {
    start = s_fogStart;
    end   = s_fogEnd;
}

void GlStateManager::setColorMask(bool r, bool g, bool b, bool a) {
    if (s_colorMaskR == r && s_colorMaskG == g && s_colorMaskB == b && s_colorMaskA == a) return;
    s_colorMaskR = r;
    s_colorMaskG = g;
    s_colorMaskB = b;
    s_colorMaskA = a;
    RenderCommand::setColorMask(r, g, b, a);
}

void GlStateManager::enablePolygonOffsetFill() {
    if (s_polygonOffsetFill) return;
    s_polygonOffsetFill = true;
    RenderCommand::enablePolygonOffsetFill();
}

void GlStateManager::disablePolygonOffsetFill() {
    if (!s_polygonOffsetFill) return;
    s_polygonOffsetFill = false;
    RenderCommand::disablePolygonOffsetFill();
}

void GlStateManager::setPolygonOffset(float factor, float units) {
    if (s_polygonOffsetFactor == factor && s_polygonOffsetUnits == units) return;
    s_polygonOffsetFactor = factor;
    s_polygonOffsetUnits  = units;
    RenderCommand::setPolygonOffset(factor, units);
}

void GlStateManager::setPolygonMode(uint32_t face, uint32_t mode) {
    if (s_polygonFace == face && s_polygonMode == mode) return;
    s_polygonFace = face;
    s_polygonMode = mode;
    RenderCommand::setPolygonMode(face, mode);
}

void GlStateManager::setLineWidth(float width) {
    if (s_lineWidth == width) return;
    s_lineWidth = width;
    RenderCommand::setLineWidth(width);
}

void GlStateManager::setPointSize(float size) {
    if (s_pointSize == size) return;
    s_pointSize = size;
    RenderCommand::setPointSize(size);
}

void GlStateManager::pushMatrix() { s_matrixStack.push_back(s_matrix); }

void GlStateManager::popMatrix() {
    if (s_matrixStack.empty()) return;
    s_matrix = s_matrixStack.back();
    s_matrixStack.pop_back();
}

void GlStateManager::loadIdentity() { s_matrix = Mat4::identity(); }

void GlStateManager::loadMatrix(const Mat4 &mat) { s_matrix = mat; }

void GlStateManager::multMatrix(const Mat4 &mat) { s_matrix = s_matrix.multiply(mat); }

void GlStateManager::translatef(float x, float y, float z) { s_matrix = s_matrix.multiply(gsmTranslation((double) x, (double) y, (double) z)); }

void GlStateManager::scalef(float x, float y, float z) { s_matrix = s_matrix.multiply(gsmScale((double) x, (double) y, (double) z)); }

void GlStateManager::rotatef(float angleDegrees, float x, float y, float z) {
    double rad = (double) angleDegrees * (M_PI / 180.0);
    s_matrix   = s_matrix.multiply(gsmRotation(rad, (double) x, (double) y, (double) z));
}

const Mat4 &GlStateManager::getMatrix() { return s_matrix; }
