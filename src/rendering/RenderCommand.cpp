#include "RenderCommand.h"

#include <GL/glew.h>

bool RenderCommand::initialize() { return glewInit() == GLEW_OK; }

void RenderCommand::enableExperimentalFeatures() { glewExperimental = GL_TRUE; }

void RenderCommand::setViewport(int32_t x, int32_t y, int32_t width, int32_t height) { glViewport(x, y, width, height); }

void RenderCommand::setClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }

void RenderCommand::setClearDepth(double depth) { glClearDepth(depth); }

void RenderCommand::setClearStencil(int32_t value) { glClearStencil(value); }

void RenderCommand::clearColor() { glClear(GL_COLOR_BUFFER_BIT); }

void RenderCommand::clearDepth() { glClear(GL_DEPTH_BUFFER_BIT); }

void RenderCommand::clearStencil() { glClear(GL_STENCIL_BUFFER_BIT); }

void RenderCommand::clearAll() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); }

void RenderCommand::enableDepthTest() { glEnable(GL_DEPTH_TEST); }

void RenderCommand::disableDepthTest() { glDisable(GL_DEPTH_TEST); }

void RenderCommand::setDepthMask(bool enabled) { glDepthMask(enabled ? GL_TRUE : GL_FALSE); }

void RenderCommand::setDepthFunc(uint32_t func) { glDepthFunc((GLenum) func); }

void RenderCommand::setDepthRange(double nearVal, double farVal) { glDepthRange(nearVal, farVal); }

void RenderCommand::enableCull() { glEnable(GL_CULL_FACE); }

void RenderCommand::disableCull() { glDisable(GL_CULL_FACE); }

void RenderCommand::setCullFace(uint32_t mode) { glCullFace((GLenum) mode); }

void RenderCommand::setFrontFace(uint32_t mode) { glFrontFace((GLenum) mode); }

void RenderCommand::enableBlend() { glEnable(GL_BLEND); }

void RenderCommand::disableBlend() { glDisable(GL_BLEND); }

void RenderCommand::setBlendFunc(uint32_t src, uint32_t dst) { glBlendFunc((GLenum) src, (GLenum) dst); }

void RenderCommand::setBlendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcA, uint32_t dstA) { glBlendFuncSeparate((GLenum) srcRGB, (GLenum) dstRGB, (GLenum) srcA, (GLenum) dstA); }

void RenderCommand::setBlendEquation(uint32_t mode) { glBlendEquation((GLenum) mode); }

void RenderCommand::setBlendEquationSeparate(uint32_t modeRGB, uint32_t modeA) { glBlendEquationSeparate((GLenum) modeRGB, (GLenum) modeA); }

void RenderCommand::enableStencilTest() { glEnable(GL_STENCIL_TEST); }

void RenderCommand::disableStencilTest() { glDisable(GL_STENCIL_TEST); }

void RenderCommand::setStencilMask(uint32_t mask) { glStencilMask(mask); }

void RenderCommand::setStencilFunc(uint32_t func, int32_t ref, uint32_t mask) { glStencilFunc((GLenum) func, ref, mask); }

void RenderCommand::setStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass) { glStencilOp((GLenum) sfail, (GLenum) dpfail, (GLenum) dppass); }

void RenderCommand::setStencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass) { glStencilOpSeparate((GLenum) face, (GLenum) sfail, (GLenum) dpfail, (GLenum) dppass); }

void RenderCommand::setStencilFuncSeparate(uint32_t face, uint32_t func, int32_t ref, uint32_t mask) { glStencilFuncSeparate((GLenum) face, (GLenum) func, ref, mask); }

void RenderCommand::setStencilMaskSeparate(uint32_t face, uint32_t mask) { glStencilMaskSeparate((GLenum) face, mask); }

void RenderCommand::enableScissorTest() { glEnable(GL_SCISSOR_TEST); }

void RenderCommand::disableScissorTest() { glDisable(GL_SCISSOR_TEST); }

void RenderCommand::setScissor(int32_t x, int32_t y, int32_t width, int32_t height) { glScissor(x, y, width, height); }

void RenderCommand::enableDither() { glEnable(GL_DITHER); }

void RenderCommand::disableDither() { glDisable(GL_DITHER); }

void RenderCommand::enableMultisample() { glEnable(GL_MULTISAMPLE); }

void RenderCommand::disableMultisample() { glDisable(GL_MULTISAMPLE); }

void RenderCommand::setColorMask(bool r, bool g, bool b, bool a) { glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE); }

void RenderCommand::setPolygonMode(uint32_t face, uint32_t mode) { glPolygonMode((GLenum) face, (GLenum) mode); }

void RenderCommand::enablePolygonOffsetFill() { glEnable(GL_POLYGON_OFFSET_FILL); }

void RenderCommand::disablePolygonOffsetFill() { glDisable(GL_POLYGON_OFFSET_FILL); }

void RenderCommand::setPolygonOffset(float factor, float units) { glPolygonOffset(factor, units); }

void RenderCommand::setLineWidth(float width) { glLineWidth(width); }

void RenderCommand::setPointSize(float size) { glPointSize(size); }

void RenderCommand::enableSRGBFramebuffer() { glEnable(GL_FRAMEBUFFER_SRGB); }

void RenderCommand::disableSRGBFramebuffer() { glDisable(GL_FRAMEBUFFER_SRGB); }

void RenderCommand::enableFramebufferSRGB() { glEnable(GL_FRAMEBUFFER_SRGB); }

void RenderCommand::disableFramebufferSRGB() { glDisable(GL_FRAMEBUFFER_SRGB); }

void RenderCommand::enableClampToEdge() {}

void RenderCommand::disableClampToEdge() {}

void RenderCommand::pixelStorei(uint32_t pname, int32_t value) { glPixelStorei((GLenum) pname, value); }

void RenderCommand::enableVertexAttrib(uint32_t index) { glEnableVertexAttribArray(index); }

void RenderCommand::disableVertexAttrib(uint32_t index) { glDisableVertexAttribArray(index); }

void RenderCommand::setVertexAttribPointer(uint32_t index, int32_t count, uint32_t type, bool normalized, uint32_t stride, uint32_t offset) {
    glVertexAttribPointer(index, count, (GLenum) type, normalized ? GL_TRUE : GL_FALSE, (GLsizei) stride, (const void *) (uintptr_t) offset);
}

void RenderCommand::setVertexAttribIPointer(uint32_t index, int32_t count, uint32_t type, uint32_t stride, uint32_t offset) { glVertexAttribIPointer(index, count, (GLenum) type, (GLsizei) stride, (const void *) (uintptr_t) offset); }

void RenderCommand::setVertexAttribDivisor(uint32_t index, uint32_t divisor) { glVertexAttribDivisor(index, divisor); }

uint32_t RenderCommand::createVertexArray() {
    GLuint id = 0;
    glGenVertexArrays(1, &id);
    return (uint32_t) id;
}

void RenderCommand::deleteVertexArray(uint32_t id) {
    GLuint x = (GLuint) id;
    glDeleteVertexArrays(1, &x);
}

void RenderCommand::bindVertexArray(uint32_t id) { glBindVertexArray((GLuint) id); }

uint32_t RenderCommand::createBuffer() {
    GLuint id = 0;
    glGenBuffers(1, &id);
    return (uint32_t) id;
}

void RenderCommand::deleteBuffer(uint32_t id) {
    GLuint x = (GLuint) id;
    glDeleteBuffers(1, &x);
}

void RenderCommand::bindArrayBuffer(uint32_t id) { glBindBuffer(GL_ARRAY_BUFFER, (GLuint) id); }

void RenderCommand::bindElementArrayBuffer(uint32_t id) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint) id); }

void RenderCommand::bindUniformBuffer(uint32_t id) { glBindBuffer(GL_UNIFORM_BUFFER, (GLuint) id); }

void RenderCommand::bindShaderStorageBuffer(uint32_t id) { glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint) id); }

void RenderCommand::uploadArrayBuffer(const void *data, uint32_t size, uint32_t usage) { glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) size, data, (GLenum) usage); }

void RenderCommand::uploadElementArrayBuffer(const void *data, uint32_t size, uint32_t usage) { glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) size, data, (GLenum) usage); }

void RenderCommand::uploadUniformBuffer(const void *data, uint32_t size, uint32_t usage) { glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr) size, data, (GLenum) usage); }

void RenderCommand::uploadShaderStorageBuffer(const void *data, uint32_t size, uint32_t usage) { glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr) size, data, (GLenum) usage); }

void RenderCommand::bufferSubData(uint32_t target, int32_t offset, uint32_t size, const void *data) { glBufferSubData((GLenum) target, (GLintptr) offset, (GLsizeiptr) size, data); }

void RenderCommand::bindBufferBase(uint32_t target, uint32_t index, uint32_t buffer) { glBindBufferBase((GLenum) target, index, (GLuint) buffer); }

void RenderCommand::renderArrays(uint32_t mode, int32_t first, int32_t count) { glDrawArrays((GLenum) mode, first, count); }

void RenderCommand::renderElements(uint32_t mode, int32_t count, uint32_t type, uint32_t offset) { glDrawElements((GLenum) mode, count, (GLenum) type, (const void *) (uintptr_t) offset); }

void RenderCommand::renderArraysInstanced(uint32_t mode, int32_t first, int32_t count, int32_t instanceCount) { glDrawArraysInstanced((GLenum) mode, first, count, instanceCount); }

void RenderCommand::renderElementsInstanced(uint32_t mode, int32_t count, uint32_t type, uint32_t offset, int32_t instanceCount) { glDrawElementsInstanced((GLenum) mode, count, (GLenum) type, (const void *) (uintptr_t) offset, instanceCount); }

uint32_t RenderCommand::createTexture() {
    GLuint id = 0;
    glGenTextures(1, &id);
    return (uint32_t) id;
}

void RenderCommand::deleteTexture(uint32_t id) {
    GLuint x = (GLuint) id;
    glDeleteTextures(1, &x);
}

void RenderCommand::activeTexture(uint32_t slot) { glActiveTexture(GL_TEXTURE0 + slot); }

void RenderCommand::bindTexture2D(uint32_t id) { glBindTexture(GL_TEXTURE_2D, (GLuint) id); }

void RenderCommand::setTextureParameteri(uint32_t pname, int32_t value) { glTexParameteri(GL_TEXTURE_2D, (GLenum) pname, value); }

void RenderCommand::setTextureParameterf(uint32_t pname, float value) { glTexParameterf(GL_TEXTURE_2D, (GLenum) pname, value); }

void RenderCommand::uploadTexture2D(int32_t width, int32_t height, uint32_t internalFormat, uint32_t format, uint32_t type, const void *data) { glTexImage2D(GL_TEXTURE_2D, 0, (GLint) internalFormat, width, height, 0, (GLenum) format, (GLenum) type, data); }

void RenderCommand::uploadTexture2DSub(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type, const void *data) { glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, (GLenum) format, (GLenum) type, data); }

void RenderCommand::copyTexSubImage2D(int32_t x, int32_t y, int32_t width, int32_t height) { glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height); }

void RenderCommand::generateMipmap2D() { glGenerateMipmap(GL_TEXTURE_2D); }

uint32_t RenderCommand::createShader(uint32_t type) { return (uint32_t) glCreateShader((GLenum) type); }

void RenderCommand::shaderSource(uint32_t shader, const char *source) { glShaderSource((GLuint) shader, 1, &source, nullptr); }

void RenderCommand::compileShader(uint32_t shader) { glCompileShader((GLuint) shader); }

bool RenderCommand::getShaderCompileStatus(uint32_t shader) {
    GLint ok = 0;
    glGetShaderiv((GLuint) shader, GL_COMPILE_STATUS, &ok);
    return ok == GL_TRUE;
}

int32_t RenderCommand::getShaderInfoLog(uint32_t shader, char *out, int32_t maxLen) {
    if (!out || maxLen <= 0) return 0;
    GLsizei written = 0;
    glGetShaderInfoLog((GLuint) shader, (GLsizei) maxLen, &written, out);
    return (int32_t) written;
}

void RenderCommand::deleteShader(uint32_t shader) { glDeleteShader((GLuint) shader); }

uint32_t RenderCommand::createProgram() { return (uint32_t) glCreateProgram(); }

void RenderCommand::attachShader(uint32_t program, uint32_t shader) { glAttachShader((GLuint) program, (GLuint) shader); }

void RenderCommand::linkProgram(uint32_t program) { glLinkProgram((GLuint) program); }

bool RenderCommand::getProgramLinkStatus(uint32_t program) {
    GLint ok = 0;
    glGetProgramiv((GLuint) program, GL_LINK_STATUS, &ok);
    return ok == GL_TRUE;
}

int32_t RenderCommand::getProgramInfoLog(uint32_t program, char *out, int32_t maxLen) {
    if (!out || maxLen <= 0) return 0;
    GLsizei written = 0;
    glGetProgramInfoLog((GLuint) program, (GLsizei) maxLen, &written, out);
    return (int32_t) written;
}

void RenderCommand::useProgram(uint32_t program) { glUseProgram((GLuint) program); }

void RenderCommand::deleteProgram(uint32_t program) { glDeleteProgram((GLuint) program); }

void RenderCommand::bindAttribLocation(uint32_t program, uint32_t index, const char *name) { glBindAttribLocation((GLuint) program, index, name); }

int32_t RenderCommand::getUniformLocation(uint32_t program, const char *name) { return (int32_t) glGetUniformLocation((GLuint) program, name); }

int32_t RenderCommand::getAttribLocation(uint32_t program, const char *name) { return (int32_t) glGetAttribLocation((GLuint) program, name); }

void RenderCommand::setUniform1i(int32_t location, int32_t v0) { glUniform1i(location, v0); }

void RenderCommand::setUniform2i(int32_t location, int32_t v0, int32_t v1) { glUniform2i(location, v0, v1); }

void RenderCommand::setUniform3i(int32_t location, int32_t v0, int32_t v1, int32_t v2) { glUniform3i(location, v0, v1, v2); }

void RenderCommand::setUniform4i(int32_t location, int32_t v0, int32_t v1, int32_t v2, int32_t v3) { glUniform4i(location, v0, v1, v2, v3); }

void RenderCommand::setUniform1ui(int32_t location, uint32_t v0) { glUniform1ui(location, v0); }

void RenderCommand::setUniform2ui(int32_t location, uint32_t v0, uint32_t v1) { glUniform2ui(location, v0, v1); }

void RenderCommand::setUniform3ui(int32_t location, uint32_t v0, uint32_t v1, uint32_t v2) { glUniform3ui(location, v0, v1, v2); }

void RenderCommand::setUniform4ui(int32_t location, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3) { glUniform4ui(location, v0, v1, v2, v3); }

void RenderCommand::setUniform1f(int32_t location, float v0) { glUniform1f(location, v0); }

void RenderCommand::setUniform2f(int32_t location, float v0, float v1) { glUniform2f(location, v0, v1); }

void RenderCommand::setUniform3f(int32_t location, float v0, float v1, float v2) { glUniform3f(location, v0, v1, v2); }

void RenderCommand::setUniform4f(int32_t location, float v0, float v1, float v2, float v3) { glUniform4f(location, v0, v1, v2, v3); }

void RenderCommand::setUniform1iv(int32_t location, int32_t count, const int32_t *value) { glUniform1iv(location, count, value); }

void RenderCommand::setUniform2iv(int32_t location, int32_t count, const int32_t *value) { glUniform2iv(location, count, value); }

void RenderCommand::setUniform3iv(int32_t location, int32_t count, const int32_t *value) { glUniform3iv(location, count, value); }

void RenderCommand::setUniform4iv(int32_t location, int32_t count, const int32_t *value) { glUniform4iv(location, count, value); }

void RenderCommand::setUniform1uiv(int32_t location, int32_t count, const uint32_t *value) { glUniform1uiv(location, count, value); }

void RenderCommand::setUniform2uiv(int32_t location, int32_t count, const uint32_t *value) { glUniform2uiv(location, count, value); }

void RenderCommand::setUniform3uiv(int32_t location, int32_t count, const uint32_t *value) { glUniform3uiv(location, count, value); }

void RenderCommand::setUniform4uiv(int32_t location, int32_t count, const uint32_t *value) { glUniform4uiv(location, count, value); }

void RenderCommand::setUniform1fv(int32_t location, int32_t count, const float *value) { glUniform1fv(location, count, value); }

void RenderCommand::setUniform2fv(int32_t location, int32_t count, const float *value) { glUniform2fv(location, count, value); }

void RenderCommand::setUniform3fv(int32_t location, int32_t count, const float *value) { glUniform3fv(location, count, value); }

void RenderCommand::setUniform4fv(int32_t location, int32_t count, const float *value) { glUniform4fv(location, count, value); }

void RenderCommand::setUniformMatrix2fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix2fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix3fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix4fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix2x3fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix2x3fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix3x2fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix3x2fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix2x4fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix2x4fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix4x2fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix4x2fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix3x4fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix3x4fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

void RenderCommand::setUniformMatrix4x3fv(int32_t location, int32_t count, bool transpose, const float *value) { glUniformMatrix4x3fv(location, count, transpose ? GL_TRUE : GL_FALSE, value); }

uint32_t RenderCommand::createFramebuffer() {
    GLuint id = 0;
    glGenFramebuffers(1, &id);
    return (uint32_t) id;
}

void RenderCommand::deleteFramebuffer(uint32_t id) {
    GLuint x = (GLuint) id;
    glDeleteFramebuffers(1, &x);
}

void RenderCommand::bindFramebuffer(uint32_t target, uint32_t id) { glBindFramebuffer((GLenum) target, (GLuint) id); }

void RenderCommand::framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int32_t level) { glFramebufferTexture2D((GLenum) target, (GLenum) attachment, (GLenum) textarget, (GLuint) texture, level); }

uint32_t RenderCommand::checkFramebufferStatus(uint32_t target) { return (uint32_t) glCheckFramebufferStatus((GLenum) target); }

uint32_t RenderCommand::createRenderbuffer() {
    GLuint id = 0;
    glGenRenderbuffers(1, &id);
    return (uint32_t) id;
}

void RenderCommand::deleteRenderbuffer(uint32_t id) {
    GLuint x = (GLuint) id;
    glDeleteRenderbuffers(1, &x);
}

void RenderCommand::bindRenderbuffer(uint32_t id) { glBindRenderbuffer(GL_RENDERBUFFER, (GLuint) id); }

void RenderCommand::renderbufferStorage(uint32_t internalFormat, int32_t width, int32_t height) { glRenderbufferStorage(GL_RENDERBUFFER, (GLenum) internalFormat, width, height); }

void RenderCommand::framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget, uint32_t renderbuffer) { glFramebufferRenderbuffer((GLenum) target, (GLenum) attachment, (GLenum) renderbuffertarget, (GLuint) renderbuffer); }
