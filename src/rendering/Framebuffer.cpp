#include "Framebuffer.h"

#include <glad/glad.h>

#include "RenderCommand.h"

Framebuffer::Framebuffer(int width, int height) : m_fbo(0), m_colorTexture(0), m_depthTexture(0), m_width(0), m_height(0) { create(width, height); }

Framebuffer::~Framebuffer() { destroy(); }

void Framebuffer::resize(int width, int height) {
    if (m_width == width && m_height == height) return;
    destroy();
    create(width, height);
}

void Framebuffer::bind() const { RenderCommand::bindFramebuffer(GL_FRAMEBUFFER, m_fbo); }

void Framebuffer::unbind() const { RenderCommand::bindFramebuffer(GL_FRAMEBUFFER, 0); }

uint32_t Framebuffer::getColorTexture() const { return m_colorTexture; }

uint32_t Framebuffer::getDepthTexture() const { return m_depthTexture; }

int Framebuffer::getWidth() const { return m_width; }

int Framebuffer::getHeight() const { return m_height; }

void Framebuffer::create(int width, int height) {
    m_width  = width;
    m_height = height;

    m_fbo = RenderCommand::createFramebuffer();
    RenderCommand::bindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    m_colorTexture = RenderCommand::createTexture();
    RenderCommand::activeTexture(0);
    RenderCommand::bindTexture2D(m_colorTexture);
    RenderCommand::uploadTexture2D(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    RenderCommand::framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    m_depthTexture = RenderCommand::createTexture();
    RenderCommand::bindTexture2D(m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    RenderCommand::framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);

    RenderCommand::bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::destroy() {
    if (m_depthTexture) {
        RenderCommand::deleteTexture(m_depthTexture);
        m_depthTexture = 0;
    }
    if (m_colorTexture) {
        RenderCommand::deleteTexture(m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_fbo) {
        RenderCommand::deleteFramebuffer(m_fbo);
        m_fbo = 0;
    }
}
