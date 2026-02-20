#include "Texture.h"

#include <cstring>
#include <stdexcept>

#include <glad/glad.h>
#include <stb_image.h>

#include "../core/Logger.h"
#include "RenderCommand.h"

Texture::Texture(const char *path) : m_id(0), m_pixelWidth(0), m_pixelHeight(0) {
    int width;
    int height;
    int channels;
    uint8_t *data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) throw std::runtime_error(std::string("failed to load texture: " + std::string(path)));

    m_pixelWidth  = width;
    m_pixelHeight = height;
    m_pixels.resize((size_t) width * (size_t) height * 4);
    std::memcpy(m_pixels.data(), data, m_pixels.size());

    m_id = RenderCommand::createTexture();
    RenderCommand::bindTexture2D(m_id);

    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_REPEAT);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_REPEAT);

    RenderCommand::uploadTexture2D(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, data);
    RenderCommand::generateMipmap2D();

    stbi_image_free(data);

    Logger::logInfo("Loaded texture '%s' (ID: %u, %dx%d)", path, m_id, width, height);
}

Texture::~Texture() { RenderCommand::deleteTexture(m_id); }

void Texture::bind(uint32_t slot) const {
    RenderCommand::activeTexture(slot);
    RenderCommand::bindTexture2D(m_id);
}

void Texture::samplePixel(int pixelX, int pixelY, float &r, float &g, float &b) const {
    if (m_pixels.empty()) {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        return;
    }

    if (pixelX < 0) pixelX = 0;
    if (pixelY < 0) pixelY = 0;
    if (pixelX >= m_pixelWidth) pixelX = m_pixelWidth - 1;
    if (pixelY >= m_pixelHeight) pixelY = m_pixelHeight - 1;

    size_t offset = ((size_t) pixelY * (size_t) m_pixelWidth + (size_t) pixelX) * 4;
    r             = (float) m_pixels[offset + 0] / 255.0f;
    g             = (float) m_pixels[offset + 1] / 255.0f;
    b             = (float) m_pixels[offset + 2] / 255.0f;
}

uint32_t Texture::getId() const { return m_id; }

int Texture::getPixelWidth() const { return m_pixelWidth; }

int Texture::getPixelHeight() const { return m_pixelHeight; }
