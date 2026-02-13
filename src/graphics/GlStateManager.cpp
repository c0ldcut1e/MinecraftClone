#include "GlStateManager.h"

#include <cmath>

#include "RenderCommand.h"

std::vector<Mat4> GlStateManager::s_matrixStack;
Mat4 GlStateManager::s_matrix = Mat4::identity();

bool GlStateManager::s_depthTest     = true;
bool GlStateManager::s_depthMask     = true;
uint32_t GlStateManager::s_depthFunc = RC_LESS;

bool GlStateManager::s_cull          = true;
uint32_t GlStateManager::s_cullFace  = RC_BACK;
uint32_t GlStateManager::s_frontFace = RC_CCW;

bool GlStateManager::s_blend        = false;
uint32_t GlStateManager::s_blendSrc = RC_BLEND_SRC_ALPHA;
uint32_t GlStateManager::s_blendDst = RC_BLEND_ONE_MINUS_SRC_ALPHA;

bool GlStateManager::s_stencil         = false;
uint32_t GlStateManager::s_stencilMask = 0xFFFFFFFF;

bool GlStateManager::s_scissor     = false;
bool GlStateManager::s_dither      = false;
bool GlStateManager::s_multisample = true;

bool GlStateManager::s_colorMaskR = true;
bool GlStateManager::s_colorMaskG = true;
bool GlStateManager::s_colorMaskB = true;
bool GlStateManager::s_colorMaskA = true;

uint32_t GlStateManager::s_polygonFace = RC_FRONT_AND_BACK;
uint32_t GlStateManager::s_polygonMode = RC_FILL;

float GlStateManager::s_lineWidth = 1.0f;
float GlStateManager::s_pointSize = 1.0f;

static inline Mat4 gsmTranslation(double x, double y, double z) {
    Mat4 matrix        = Mat4::identity();
    double *matrixData = (double *) matrix.data();
    matrixData[12]     = x;
    matrixData[13]     = y;
    matrixData[14]     = z;
    return matrix;
}

static inline Mat4 gsmScale(double x, double y, double z) {
    Mat4 matrix        = Mat4::identity();
    double *matrixData = (double *) matrix.data();
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
    double *matrixData = (double *) matrix.data();

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
    s_depthTest = true;
    RenderCommand::enableDepthTest();
}

void GlStateManager::disableDepthTest() {
    s_depthTest = false;
    RenderCommand::disableDepthTest();
}

void GlStateManager::setDepthFunc(uint32_t func) {
    s_depthFunc = func;
    RenderCommand::setDepthFunc(func);
}

void GlStateManager::setDepthMask(bool enabled) {
    s_depthMask = enabled;
    RenderCommand::setDepthMask(enabled);
}

void GlStateManager::enableCull() {
    s_cull = true;
    RenderCommand::enableCull();
}

void GlStateManager::disableCull() {
    s_cull = false;
    RenderCommand::disableCull();
}

void GlStateManager::setCullFace(uint32_t face) {
    s_cullFace = face;
    RenderCommand::setCullFace(face);
}

void GlStateManager::setFrontFace(uint32_t winding) {
    s_frontFace = winding;
    RenderCommand::setFrontFace(winding);
}

void GlStateManager::enableBlend() {
    s_blend = true;
    RenderCommand::enableBlend();
}

void GlStateManager::disableBlend() {
    s_blend = false;
    RenderCommand::disableBlend();
}

void GlStateManager::setBlendFunc(uint32_t src, uint32_t dst) {
    s_blendSrc = src;
    s_blendDst = dst;
    RenderCommand::setBlendFunc(src, dst);
}

void GlStateManager::enableStencilTest() {
    s_stencil = true;
    RenderCommand::enableStencilTest();
}

void GlStateManager::disableStencilTest() {
    s_stencil = false;
    RenderCommand::disableStencilTest();
}

void GlStateManager::setStencilMask(uint32_t mask) {
    s_stencilMask = mask;
    RenderCommand::setStencilMask(mask);
}

void GlStateManager::setStencilFunc(uint32_t func, int ref, uint32_t mask) { RenderCommand::setStencilFunc(func, ref, mask); }

void GlStateManager::setStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass) { RenderCommand::setStencilOp(sfail, dpfail, dppass); }

void GlStateManager::enableScissorTest() {
    s_scissor = true;
    RenderCommand::enableScissorTest();
}

void GlStateManager::disableScissorTest() {
    s_scissor = false;
    RenderCommand::disableScissorTest();
}

void GlStateManager::setScissor(int x, int y, int w, int h) { RenderCommand::setScissor(x, y, w, h); }

void GlStateManager::enableDither() {
    s_dither = true;
    RenderCommand::enableDither();
}

void GlStateManager::disableDither() {
    s_dither = false;
    RenderCommand::disableDither();
}

void GlStateManager::enableMultisample() {
    s_multisample = true;
    RenderCommand::enableMultisample();
}

void GlStateManager::disableMultisample() {
    s_multisample = false;
    RenderCommand::disableMultisample();
}

void GlStateManager::setColorMask(bool r, bool g, bool b, bool a) {
    if (s_colorMaskR == r && s_colorMaskG == g && s_colorMaskB == b && s_colorMaskA == a) return;
    s_colorMaskR = r;
    s_colorMaskG = g;
    s_colorMaskB = b;
    s_colorMaskA = a;
    RenderCommand::setColorMask(r, g, b, a);
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
