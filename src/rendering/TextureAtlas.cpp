#include "TextureAtlas.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

#include "../core/Logger.h"

TextureAtlas::TextureAtlas() {}

void TextureAtlas::build(TextureRepository *repository, const std::vector<std::string> &paths)
{
    m_regions.clear();
    m_texture.reset();

    if (!repository || paths.empty())
    {
        return;
    }

    int cellWidth  = 0;
    int cellHeight = 0;
    std::vector<std::shared_ptr<Texture>> textures;
    textures.reserve(paths.size());

    for (const std::string &path : paths)
    {
        std::shared_ptr<Texture> texture = repository->get(path);
        if (!texture)
        {
            continue;
        }

        textures.push_back(texture);
        if (texture->getPixelWidth() > cellWidth)
        {
            cellWidth = texture->getPixelWidth();
        }
        if (texture->getPixelHeight() > cellHeight)
        {
            cellHeight = texture->getPixelHeight();
        }
    }

    if (textures.empty() || cellWidth <= 0 || cellHeight <= 0)
    {
        return;
    }

    int count   = (int) textures.size();
    int columns = (int) ceil(sqrt((double) count));
    int rows    = (count + columns - 1) / columns;
    int width   = columns * cellWidth;
    int height  = rows * cellHeight;

    std::vector<uint8_t> atlasPixels((size_t) width * (size_t) height * 4, 0);

    for (int i = 0; i < count; i++)
    {
        const std::string &path           = paths[(size_t) i];
        const std::shared_ptr<Texture> &texture = textures[(size_t) i];

        int col = i % columns;
        int row = i / columns;
        int dstX = col * cellWidth;
        int dstY = row * cellHeight;
        int srcWidth = texture->getPixelWidth();
        int srcHeight = texture->getPixelHeight();
        const std::vector<uint8_t> &srcPixels = texture->getPixels();

        for (int y = 0; y < srcHeight; y++)
        {
            size_t srcOffset = (size_t) y * (size_t) srcWidth * 4;
            size_t dstOffset = ((size_t) (dstY + y) * (size_t) width + (size_t) dstX) * 4;
            memcpy(atlasPixels.data() + dstOffset, srcPixels.data() + srcOffset,
                   (size_t) srcWidth * 4);
        }

        Region region;
        region.u0 = (float) dstX / (float) width;
        region.v0 = (float) dstY / (float) height;
        region.u1 = (float) (dstX + srcWidth) / (float) width;
        region.v1 = (float) (dstY + srcHeight) / (float) height;
        m_regions[path] = region;
    }

    m_texture = std::make_shared<Texture>(width, height, atlasPixels.data(), true);
    Logger::logInfo("Built texture atlas (%dx%d, %d entries)", width, height, count);
}

bool TextureAtlas::has(const std::string &path) const
{
    return m_regions.find(path) != m_regions.end();
}

TextureAtlas::Region TextureAtlas::get(const std::string &path) const
{
    std::unordered_map<std::string, Region>::const_iterator it = m_regions.find(path);
    if (it == m_regions.end())
    {
        throw std::runtime_error("missing atlas region");
    }
    return it->second;
}

Texture *TextureAtlas::getTexture() const { return m_texture.get(); }
