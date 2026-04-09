#include "Font.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glad/glad.h>

#include "../utils/math/Mth.h"
#include "GlStateManager.h"
#include "RenderCommand.h"

static constexpr float BITMAP_FONT_SCALE_SNAP_STEPS = 8.0f;

static inline float snapBitmapPixel(float value, bool nearest)
{
    return nearest ? std::round(value) : value;
}

static inline uint32_t mulColor(uint32_t argb, float mul)
{
    uint32_t a = (argb >> 24) & 0xFF;
    uint32_t r = (argb >> 16) & 0xFF;
    uint32_t g = (argb >> 8) & 0xFF;
    uint32_t b = argb & 0xFF;
    r          = (uint32_t) (r * mul);
    g          = (uint32_t) (g * mul);
    b          = (uint32_t) (b * mul);
    if (r > 255)
    {
        r = 255;
    }
    if (g > 255)
    {
        g = 255;
    }
    if (b > 255)
    {
        b = 255;
    }
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline void colorToFloats(uint32_t argb, float *r, float *g, float *b, float *a)
{
    *a = ((argb >> 24) & 0xFF) / 255.0f;
    *r = ((argb >> 16) & 0xFF) / 255.0f;
    *g = ((argb >> 8) & 0xFF) / 255.0f;
    *b = (argb & 0xFF) / 255.0f;
}

Font::Font(const char *fontPath, int pixelHeight)
    : m_shader("shaders/font.vert", "shaders/font.frag"),
      m_atlas(std::make_unique<Texture>(fontPath)),
      m_screenProjection(Mat4::orthographic(0.0, 1920.0, 1080.0, 0.0, -1.0, 1.0)),
      m_levelView(Mat4::identity()), m_levelProjection(Mat4::identity()), m_vao(0), m_vbo(0),
      m_nearest(true), m_shadowOffsetX(1.0f), m_shadowOffsetY(1.0f), m_shadowOffsetZ(0.001f),
      m_ascent((float) std::max(pixelHeight - 1, 1)),
      m_lineHeight((float) std::max(pixelHeight, 1)),
      m_bitmapScale((float) std::max(pixelHeight, 1) / 8.0f), m_cellPixelWidth(8),
      m_cellPixelHeight(8), m_glyphColumns(0), m_glyphRows(0), m_fallbackCodepoint((uint32_t) '?')
{
    if (!m_atlas || m_atlas->getPixelWidth() <= 0 || m_atlas->getPixelHeight() <= 0)
    {
        throw std::runtime_error("failed to load bitmap font");
    }

    m_glyphColumns = m_atlas->getPixelWidth() / m_cellPixelWidth;
    m_glyphRows    = m_atlas->getPixelHeight() / m_cellPixelHeight;
    if (m_glyphColumns <= 0 || m_glyphRows <= 0)
    {
        throw std::runtime_error("invalid bitmap font atlas size");
    }

    m_ascent     = (float) (m_cellPixelHeight - 1) * m_bitmapScale;
    m_lineHeight = (float) m_cellPixelHeight * m_bitmapScale;
    loadBitmapGlyphs();

    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::uploadArrayBuffer(nullptr, (uint32_t) (6 * 5 * sizeof(float)), GL_DYNAMIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(float),
                                          3 * sizeof(float));

    m_shader.bind();
    m_shader.setInt("u_font", 0);
    setNearest(m_nearest);
}

Font::~Font()
{
    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);
}

void Font::setScreenProjection(const Mat4 &projection) { m_screenProjection = projection; }

void Font::setLevelViewProjection(const Mat4 &view, const Mat4 &projection)
{
    m_levelView       = view;
    m_levelProjection = projection;
}

void Font::setNearest(bool nearest)
{
    m_nearest = nearest;

    if (!m_atlas)
    {
        return;
    }

    RenderCommand::bindTexture2D(m_atlas->getId());

    uint32_t filter = m_nearest ? GL_NEAREST : GL_LINEAR;
    RenderCommand::setTextureParameteri(GL_TEXTURE_MIN_FILTER, filter);
    RenderCommand::setTextureParameteri(GL_TEXTURE_MAG_FILTER, filter);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Font::setShadowOffset(float x, float y, float z)
{
    m_shadowOffsetX = x;
    m_shadowOffsetY = y;
    m_shadowOffsetZ = z;
}

float Font::getWidth(std::wstring_view text, float scale) const
{
    if (text.empty())
    {
        return 0.0f;
    }

    float width = 0.0f;

    for (wchar_t wc : text)
    {
        if (wc == L'\n')
        {
            break;
        }

        uint32_t cp = (uint32_t) wc;

        const Glyph &glyph = getGlyph(cp);
        if (glyph.advance <= 0.0f)
        {
            continue;
        }

        width += glyph.advance * m_bitmapScale * scale;
    }

    return width;
}

float Font::getAscent(float scale) const { return m_ascent * scale; }

float Font::getLineHeight(float scale) const { return m_lineHeight * scale; }

float Font::snapScale(float scale) const
{
    if (m_bitmapScale <= 0.0f)
    {
        return scale;
    }

    float snappedGlyphScale =
            std::max(1.0f, std::round(scale * m_bitmapScale * BITMAP_FONT_SCALE_SNAP_STEPS) /
                                   BITMAP_FONT_SCALE_SNAP_STEPS);
    return snappedGlyphScale / m_bitmapScale;
}

void Font::draw(std::wstring_view text, float x, float y, float scale, uint32_t argb)
{
    if (text.empty())
    {
        return;
    }

    m_shader.setMat4("u_model", GlStateManager::getMatrix().data);

    float r;
    float g;
    float b;
    float a;
    colorToFloats(argb, &r, &g, &b, &a);

    GlStateManager::disableDepthTest();
    GlStateManager::setDepthMask(false);

    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.setMat4("u_view", Mat4::identity().data);
    m_shader.setMat4("u_projection", m_screenProjection.data);
    m_shader.setMat4("u_model", GlStateManager::getMatrix().data);
    m_shader.setVec3("u_color", r, g, b);
    m_shader.setFloat("u_alpha", a);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    if (m_atlas)
    {
        m_atlas->bind(0);
    }

    float glyphScale  = scale * m_bitmapScale;
    float logicalPenX = x;
    float logicalPenY = y;

    for (wchar_t wc : text)
    {
        uint32_t cp = (uint32_t) wc;
        if (cp == (uint32_t) L'\n')
        {
            logicalPenX = x;
            logicalPenY += m_lineHeight * scale;
            continue;
        }

        const Glyph &ch = getGlyph(cp);
        if (ch.advance <= 0.0f)
        {
            continue;
        }

        float left = snapBitmapPixel(logicalPenX + ch.bearingX * glyphScale, m_nearest);
        float top  = snapBitmapPixel(logicalPenY - ch.bearingY * glyphScale, m_nearest);
        float right =
                snapBitmapPixel(logicalPenX + (ch.bearingX + ch.width) * glyphScale, m_nearest);
        float bottom =
                snapBitmapPixel(logicalPenY + (ch.height - ch.bearingY) * glyphScale, m_nearest);
        float width  = std::max(0.0f, right - left);
        float height = std::max(0.0f, bottom - top);

        float vertices[6][5] = {
                {left, top, 0.0f, ch.u0, ch.v0},
                {left + width, top, 0.0f, ch.u1, ch.v0},
                {left + width, top + height, 0.0f, ch.u1, ch.v1},
                {left, top, 0.0f, ch.u0, ch.v0},
                {left + width, top + height, 0.0f, ch.u1, ch.v1},
                {left, top + height, 0.0f, ch.u0, ch.v1},
        };

        RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), GL_DYNAMIC_DRAW);
        RenderCommand::renderArrays(GL_TRIANGLES, 0, 6);

        logicalPenX += ch.advance * glyphScale;
    }

    GlStateManager::setDepthMask(true);
    GlStateManager::enableDepthTest();
}

void Font::drawShadow(std::wstring_view text, float x, float y, float scale, uint32_t argb)
{
    uint32_t shadow = mulColor(argb, 0.25f);
    draw(text, x + m_shadowOffsetX * scale, y + m_shadowOffsetY * scale, scale, shadow);
    draw(text, x, y, scale, argb);
}

void Font::levelDraw(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb)
{
    if (text.empty())
    {
        return;
    }

    m_shader.setMat4("u_model", GlStateManager::getMatrix().data);

    float r;
    float g;
    float b;
    float a;
    colorToFloats(argb, &r, &g, &b, &a);

    GlStateManager::setDepthMask(false);

    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.setMat4("u_view", m_levelView.data);
    m_shader.setMat4("u_projection", m_levelProjection.data);
    m_shader.setMat4("u_model", GlStateManager::getMatrix().data);
    m_shader.setVec3("u_color", r, g, b);
    m_shader.setFloat("u_alpha", a);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);
    if (m_atlas)
    {
        m_atlas->bind(0);
    }

    float glyphScale = scale * m_bitmapScale;
    float penX       = 0.0f;
    float penY       = 0.0f;

    for (wchar_t wc : text)
    {
        uint32_t cp = (uint32_t) wc;
        if (cp == (uint32_t) L'\n')
        {
            penX = 0.0f;
            penY -= m_lineHeight * scale;
            continue;
        }

        const Glyph &ch = getGlyph(cp);
        if (ch.advance <= 0.0f)
        {
            continue;
        }

        float xPos = penX + ch.bearingX * glyphScale;
        float yTop = penY + ch.bearingY * glyphScale;

        float width  = ch.width * glyphScale;
        float height = ch.height * glyphScale;

        float x0 = pos.x + xPos;
        float y0 = pos.y + yTop;
        float z0 = pos.z;

        float vertices[6][5] = {
                {x0, y0, z0, ch.u0, ch.v0},
                {x0 + width, y0, z0, ch.u1, ch.v0},
                {x0 + width, y0 - height, z0, ch.u1, ch.v1},
                {x0, y0, z0, ch.u0, ch.v0},
                {x0 + width, y0 - height, z0, ch.u1, ch.v1},
                {x0, y0 - height, z0, ch.u0, ch.v1},
        };

        RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), GL_DYNAMIC_DRAW);
        RenderCommand::renderArrays(GL_TRIANGLES, 0, 6);

        penX += ch.advance * glyphScale;
    }

    GlStateManager::setDepthMask(true);
}

void Font::levelDrawShadow(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb)
{
    uint32_t shadow = mulColor(argb, 0.25f);

    float ox = m_shadowOffsetX * scale;
    float oy = m_shadowOffsetY * scale;
    float oz = m_shadowOffsetZ;
    if (oz == 0.0f)
    {
        oz = 0.01f;
    }
    oz *= scale;

    levelDraw(text, Vec3(pos.x + ox, pos.y - oy, pos.z - oz), scale, shadow);
    levelDraw(text, pos, scale, argb);
}

const Font::Glyph &Font::getGlyph(uint32_t codepoint) const
{
    if (codepoint < m_glyphs.size())
    {
        return m_glyphs[codepoint];
    }

    if (m_fallbackCodepoint < m_glyphs.size())
    {
        return m_glyphs[m_fallbackCodepoint];
    }

    static const Glyph empty{};
    return empty;
}

void Font::loadBitmapGlyphs()
{
    if (!m_atlas)
    {
        return;
    }

    m_glyphs.assign((size_t) m_glyphColumns * (size_t) m_glyphRows, Glyph{});

    const std::vector<uint8_t> &sourcePixels = m_atlas->getPixels();
    int sourceAtlasWidth                     = m_atlas->getPixelWidth();
    int sourceAtlasHeight                    = m_atlas->getPixelHeight();
    if (sourceAtlasWidth <= 0 || sourceAtlasHeight <= 0 || sourcePixels.empty())
    {
        return;
    }

    int paddedCellWidth  = m_cellPixelWidth + 2;
    int paddedCellHeight = m_cellPixelHeight + 2;
    int atlasWidth       = m_glyphColumns * paddedCellWidth;
    int atlasHeight      = m_glyphRows * paddedCellHeight;
    std::vector<uint8_t> paddedPixels((size_t) atlasWidth * (size_t) atlasHeight * (size_t) 4, 0);

    for (int glyphY = 0; glyphY < m_glyphRows; glyphY++)
    {
        for (int glyphX = 0; glyphX < m_glyphColumns; glyphX++)
        {
            Glyph glyph{};
            int cellMinX = m_cellPixelWidth;
            int cellMinY = m_cellPixelHeight;
            int cellMaxX = -1;
            int cellMaxY = -1;

            for (int py = 0; py < m_cellPixelHeight; py++)
            {
                for (int px = 0; px < m_cellPixelWidth; px++)
                {
                    int atlasX   = glyphX * m_cellPixelWidth + px;
                    int atlasY   = glyphY * m_cellPixelHeight + py;
                    size_t index = ((size_t) atlasY * (size_t) sourceAtlasWidth + (size_t) atlasX) *
                                   (size_t) 4;
                    if (sourcePixels[index + 3] == 0)
                    {
                        continue;
                    }

                    cellMinX = std::min(cellMinX, px);
                    cellMinY = std::min(cellMinY, py);
                    cellMaxX = std::max(cellMaxX, px);
                    cellMaxY = std::max(cellMaxY, py);
                }
            }

            uint32_t codepoint = (uint32_t) (glyphY * m_glyphColumns + glyphX);
            if (cellMaxX < cellMinX || cellMaxY < cellMinY)
            {
                glyph.advance       = codepoint == (uint32_t) ' ' ? 4.0f : 0.0f;
                m_glyphs[codepoint] = glyph;
                continue;
            }

            int glyphWidth  = cellMaxX - cellMinX + 1;
            int glyphHeight = cellMaxY - cellMinY + 1;
            int pixelX0     = glyphX * paddedCellWidth + 1;
            int pixelY0     = glyphY * paddedCellHeight + 1;
            int pixelX1     = pixelX0 + glyphWidth;
            int pixelY1     = pixelY0 + glyphHeight;

            for (int py = 0; py < glyphHeight; py++)
            {
                for (int px = 0; px < glyphWidth; px++)
                {
                    int sourceX = glyphX * m_cellPixelWidth + cellMinX + px;
                    int sourceY = glyphY * m_cellPixelHeight + cellMinY + py;
                    int targetX = pixelX0 + px;
                    int targetY = pixelY0 + py;
                    size_t sourceIndex =
                            ((size_t) sourceY * (size_t) sourceAtlasWidth + (size_t) sourceX) *
                            (size_t) 4;
                    size_t targetIndex =
                            ((size_t) targetY * (size_t) atlasWidth + (size_t) targetX) *
                            (size_t) 4;
                    paddedPixels[targetIndex + 0] = sourcePixels[sourceIndex + 0];
                    paddedPixels[targetIndex + 1] = sourcePixels[sourceIndex + 1];
                    paddedPixels[targetIndex + 2] = sourcePixels[sourceIndex + 2];
                    paddedPixels[targetIndex + 3] = sourcePixels[sourceIndex + 3];
                }
            }

            glyph.u0       = (float) pixelX0 / (float) atlasWidth;
            glyph.v0       = (float) pixelY0 / (float) atlasHeight;
            glyph.u1       = (float) pixelX1 / (float) atlasWidth;
            glyph.v1       = (float) pixelY1 / (float) atlasHeight;
            glyph.width    = (float) glyphWidth;
            glyph.height   = (float) glyphHeight;
            glyph.bearingX = (float) cellMinX;
            glyph.bearingY = (float) ((m_cellPixelHeight - 1) - cellMinY);
            glyph.advance  = (float) Mth::clampf(cellMaxX + 2, 2, m_cellPixelWidth);

            m_glyphs[codepoint] = glyph;
        }
    }

    if ((size_t) ' ' < m_glyphs.size() && m_glyphs[(size_t) ' '].advance <= 0.0f)
    {
        m_glyphs[(size_t) ' '].advance = 4.0f;
    }

    m_atlas = std::make_unique<Texture>(atlasWidth, atlasHeight, paddedPixels.data(), true);
    setNearest(m_nearest);
}
