#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../utils/math/Mat4.h"
#include "../utils/math/Vec3.h"
#include "Shader.h"

struct FT_LibraryRec_;
struct FT_FaceRec_;
using FT_Library = FT_LibraryRec_ *;
using FT_Face    = FT_FaceRec_ *;

class Font {
public:
    Font(const char *ttfPath, int pixelHeight);
    ~Font();

    Font(const Font &)            = delete;
    Font &operator=(const Font &) = delete;

    void setScreenProjection(const Mat4 &projection);
    void setWorldViewProjection(const Mat4 &view, const Mat4 &projection);

    void setNearest(bool nearest);
    void setShadowOffset(float x, float y, float z);

    float getWidth(std::wstring_view text, float scale) const;

    void render(std::wstring_view text, float x, float y, float scale, uint32_t argb);
    void renderShadow(std::wstring_view text, float x, float y, float scale, uint32_t argb);

    void worldRender(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb);
    void worldRenderShadow(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb);

private:
    struct Glyph {
        uint32_t texture = 0;
        int width        = 0;
        int height       = 0;
        int bearingX     = 0;
        int bearingY     = 0;
        uint32_t advance = 0;
    };

    const Glyph &getOrCreateGlyph(uint32_t codepoint);

    Shader m_shader;

    FT_Library m_ft;
    FT_Face m_face;

    Mat4 m_screenProjection;
    Mat4 m_worldView;
    Mat4 m_worldProjection;

    uint32_t m_vao;
    uint32_t m_vbo;

    bool m_nearest;

    float m_shadowOffsetX;
    float m_shadowOffsetY;
    float m_shadowOffsetZ;

    std::unordered_map<uint32_t, Glyph> m_glyphs;
};
