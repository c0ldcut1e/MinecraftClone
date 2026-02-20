#include "Texture.h"

#include <stdexcept>

#include <glad/glad.h>
#include <stb_image.h>

#include "../core/Logger.h"
#include "RenderCommand.h"

Texture::Texture(const char *path) : m_id(0) {
    int width;
    int height;
    int channels;

    uint8_t *data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) throw std::runtime_error("failed to load texture");

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
