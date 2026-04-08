#pragma once

#include <cstdint>

class Framebuffer
{
public:
    Framebuffer(int width, int height);
    Framebuffer(int width, int height, uint32_t colorInternalFormat, uint32_t colorFormat,
                uint32_t colorType);
    ~Framebuffer();

    void resize(int width, int height);

    void bind() const;
    void unbind() const;

    uint32_t getId() const;
    uint32_t getColorTexture() const;
    uint32_t getDepthTexture() const;

    int getWidth() const;
    int getHeight() const;

private:
    void create(int width, int height);
    void destroy();

    uint32_t m_fbo;
    uint32_t m_colorTexture;
    uint32_t m_depthTexture;
    uint32_t m_colorInternalFormat;
    uint32_t m_colorFormat;
    uint32_t m_colorType;

    int m_width;
    int m_height;
};
