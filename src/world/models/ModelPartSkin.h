#pragma once

#include <string>

class ModelPartSkin {
public:
    ModelPartSkin();
    ModelPartSkin(const std::string &texturePath, int textureWidth, int textureHeight);

    const std::string &getTexturePath() const;
    int getTextureWidth() const;
    int getTextureHeight() const;

private:
    std::string m_texturePath;
    int m_textureWidth;
    int m_textureHeight;
};
