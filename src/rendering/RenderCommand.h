#pragma once

#include <cstddef>
#include <cstdint>

class RenderCommand {
public:
    static bool initialize();
    static void enableExperimentalFeatures();

    static void setViewport(int x, int y, int width, int height);

    static void setClearColor(float r, float g, float b, float a);
    static void setClearDepth(double depth);
    static void setClearStencil(int value);

    static void clearColor();
    static void clearDepth();
    static void clearStencil();
    static void clearAll();

    static void enableDepthTest();
    static void disableDepthTest();
    static void setDepthMask(bool enabled);
    static void setDepthFunc(uint32_t func);
    static void setDepthRange(double nearVal, double farVal);

    static void enableCull();
    static void disableCull();
    static void setCullFace(uint32_t mode);
    static void setFrontFace(uint32_t mode);

    static void enableBlend();
    static void disableBlend();
    static void setBlendFunc(uint32_t src, uint32_t dst);
    static void setBlendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcA, uint32_t dstA);
    static void setBlendEquation(uint32_t mode);
    static void setBlendEquationSeparate(uint32_t modeRGB, uint32_t modeA);

    static void enableStencilTest();
    static void disableStencilTest();
    static void setStencilMask(uint32_t mask);
    static void setStencilFunc(uint32_t func, int ref, uint32_t mask);
    static void setStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass);
    static void setStencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass);
    static void setStencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask);
    static void setStencilMaskSeparate(uint32_t face, uint32_t mask);

    static void enableScissorTest();
    static void disableScissorTest();
    static void setScissor(int x, int y, int width, int height);

    static void enableDither();
    static void disableDither();

    static void enableMultisample();
    static void disableMultisample();

    static void setColorMask(bool r, bool g, bool b, bool a);

    static void setPolygonMode(uint32_t face, uint32_t mode);
    static void enablePolygonOffsetFill();
    static void disablePolygonOffsetFill();
    static void setPolygonOffset(float factor, float units);

    static void setLineWidth(float width);
    static void setPointSize(float size);

    static void enableSRGBFramebuffer();
    static void disableSRGBFramebuffer();

    static void enableFramebufferSRGB();
    static void disableFramebufferSRGB();

    static void enableClampToEdge();
    static void disableClampToEdge();

    static void pixelStorei(uint32_t pname, int value);

    static void enableVertexAttrib(uint32_t index);
    static void disableVertexAttrib(uint32_t index);
    static void setVertexAttribPointer(uint32_t index, int count, uint32_t type, bool normalized, uint32_t stride, uint32_t offset);
    static void setVertexAttribIPointer(uint32_t index, int count, uint32_t type, uint32_t stride, uint32_t offset);
    static void setVertexAttribDivisor(uint32_t index, uint32_t divisor);

    static uint32_t createVertexArray();
    static void deleteVertexArray(uint32_t id);
    static void bindVertexArray(uint32_t id);

    static uint32_t createBuffer();
    static void deleteBuffer(uint32_t id);
    static void bindArrayBuffer(uint32_t id);
    static void bindElementArrayBuffer(uint32_t id);
    static void bindUniformBuffer(uint32_t id);

    static void uploadArrayBuffer(const void *data, uint32_t size, uint32_t usage);
    static void uploadElementArrayBuffer(const void *data, uint32_t size, uint32_t usage);
    static void uploadUniformBuffer(const void *data, uint32_t size, uint32_t usage);

    static void bufferSubData(uint32_t target, int offset, uint32_t size, const void *data);

    static void bindBufferBase(uint32_t target, uint32_t index, uint32_t buffer);

    static void renderArrays(uint32_t mode, int first, int count);
    static void renderElements(uint32_t mode, int count, uint32_t type, uint32_t offset);

    static void renderArraysInstanced(uint32_t mode, int first, int count, int instanceCount);
    static void renderElementsInstanced(uint32_t mode, int count, uint32_t type, uint32_t offset, int instanceCount);

    static uint32_t createTexture();
    static void deleteTexture(uint32_t id);
    static void activeTexture(uint32_t slot);
    static void bindTexture2D(uint32_t id);

    static void setTextureParameteri(uint32_t pname, int value);
    static void setTextureParameterf(uint32_t pname, float value);

    static void uploadTexture2D(int width, int height, uint32_t internalFormat, uint32_t format, uint32_t type, const void *data);
    static void uploadTexture2DSub(int x, int y, int width, int height, uint32_t format, uint32_t type, const void *data);
    static void copyTexSubImage2D(int x, int y, int width, int height);
    static void generateMipmap2D();

    static uint32_t createShader(uint32_t type);
    static void shaderSource(uint32_t shader, const char *source);
    static void compileShader(uint32_t shader);
    static bool getShaderCompileStatus(uint32_t shader);
    static int getShaderInfoLog(uint32_t shader, char *out, int maxLen);
    static void deleteShader(uint32_t shader);

    static uint32_t createProgram();
    static void attachShader(uint32_t program, uint32_t shader);
    static void linkProgram(uint32_t program);
    static bool getProgramLinkStatus(uint32_t program);
    static int getProgramInfoLog(uint32_t program, char *out, int maxLen);
    static void useProgram(uint32_t program);
    static void deleteProgram(uint32_t program);

    static void bindAttribLocation(uint32_t program, uint32_t index, const char *name);

    static int getUniformLocation(uint32_t program, const char *name);
    static int getAttribLocation(uint32_t program, const char *name);

    static void setUniform1i(int location, int v0);
    static void setUniform2i(int location, int v0, int v1);
    static void setUniform3i(int location, int v0, int v1, int v2);
    static void setUniform4i(int location, int v0, int v1, int v2, int v3);

    static void setUniform1ui(int location, uint32_t v0);
    static void setUniform2ui(int location, uint32_t v0, uint32_t v1);
    static void setUniform3ui(int location, uint32_t v0, uint32_t v1, uint32_t v2);
    static void setUniform4ui(int location, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3);

    static void setUniform1f(int location, float v0);
    static void setUniform2f(int location, float v0, float v1);
    static void setUniform3f(int location, float v0, float v1, float v2);
    static void setUniform4f(int location, float v0, float v1, float v2, float v3);

    static void setUniform1iv(int location, int count, const int *value);
    static void setUniform2iv(int location, int count, const int *value);
    static void setUniform3iv(int location, int count, const int *value);
    static void setUniform4iv(int location, int count, const int *value);

    static void setUniform1uiv(int location, int count, const uint32_t *value);
    static void setUniform2uiv(int location, int count, const uint32_t *value);
    static void setUniform3uiv(int location, int count, const uint32_t *value);
    static void setUniform4uiv(int location, int count, const uint32_t *value);

    static void setUniform1fv(int location, int count, const float *value);
    static void setUniform2fv(int location, int count, const float *value);
    static void setUniform3fv(int location, int count, const float *value);
    static void setUniform4fv(int location, int count, const float *value);

    static void setUniformMatrix2fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix3fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix4fv(int location, int count, bool transpose, const float *value);

    static void setUniformMatrix2x3fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix3x2fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix2x4fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix4x2fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix3x4fv(int location, int count, bool transpose, const float *value);
    static void setUniformMatrix4x3fv(int location, int count, bool transpose, const float *value);

    static uint32_t createFramebuffer();
    static void deleteFramebuffer(uint32_t id);
    static void bindFramebuffer(uint32_t target, uint32_t id);
    static void framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int level);
    static uint32_t checkFramebufferStatus(uint32_t target);

    static uint32_t createRenderbuffer();
    static void deleteRenderbuffer(uint32_t id);
    static void bindRenderbuffer(uint32_t id);
    static void renderbufferStorage(uint32_t internalFormat, int width, int height);
    static void framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget, uint32_t renderbuffer);
};
