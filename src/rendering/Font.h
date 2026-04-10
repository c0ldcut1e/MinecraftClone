#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../utils/math/Mat4.h"
#include "../utils/math/Vec3.h"
#include "Shader.h"
#include "Texture.h"

class Font
{
public:
    Font(const char *fontPath, int pixelHeight);
    ~Font();

    Font(const Font &)            = delete;
    Font &operator=(const Font &) = delete;

    void setScreenProjection(const Mat4 &projection);
    void setLevelViewProjection(const Mat4 &view, const Mat4 &projection);

    void setNearest(bool nearest);
    void setShadowOffset(float x, float y, float z);

    float getWidth(std::wstring_view text, float scale) const;
    float getAscent(float scale) const;
    float getLineHeight(float scale) const;
    float getPixelSize(float scale) const;
    float snapScale(float scale) const;

    void draw(std::wstring_view text, float x, float y, float scale, uint32_t argb);
    void drawShadow(std::wstring_view text, float x, float y, float scale, uint32_t argb);

    void levelDraw(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb);
    void levelDrawShadow(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb);

private:
    struct Glyph
    {
        float u0       = 0.0f;
        float v0       = 0.0f;
        float u1       = 0.0f;
        float v1       = 0.0f;
        float width    = 0.0f;
        float height   = 0.0f;
        float bearingX = 0.0f;
        float bearingY = 0.0f;
        float advance  = 0.0f;
    };

    const Glyph &getGlyph(uint32_t codepoint) const;
    void loadBitmapGlyphs();

    Shader m_shader;
    std::unique_ptr<Texture> m_atlas;

    Mat4 m_screenProjection;
    Mat4 m_levelView;
    Mat4 m_levelProjection;

    uint32_t m_vao;
    uint32_t m_vbo;

    bool m_nearest;

    float m_shadowOffsetX;
    float m_shadowOffsetY;
    float m_shadowOffsetZ;
    float m_ascent;
    float m_lineHeight;
    float m_bitmapScale;
    int m_cellPixelWidth;
    int m_cellPixelHeight;
    int m_glyphColumns;
    int m_glyphRows;
    uint32_t m_fallbackCodepoint;

    std::vector<Glyph> m_glyphs;
};
