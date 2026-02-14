#include "ModelPartSkin.h"

ModelPartSkin::ModelPartSkin() : m_texturePath(""), m_textureWidth(0), m_textureHeight(0) {}

ModelPartSkin::ModelPartSkin(const std::string &texturePath, int textureWidth, int textureHeight) : m_texturePath(texturePath), m_textureWidth(textureWidth), m_textureHeight(textureHeight) {}

const std::string &ModelPartSkin::getTexturePath() const { return m_texturePath; }

int ModelPartSkin::getTextureWidth() const { return m_textureWidth; }

int ModelPartSkin::getTextureHeight() const { return m_textureHeight; }
