#include "Font.h"

#include <stdexcept>
#include <vector>

#include <GL/glew.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "GlStateManager.h"
#include "RenderCommand.h"

static inline uint32_t mulColor(uint32_t argb, float mul) {
    uint32_t a = (argb >> 24) & 0xFF;
    uint32_t r = (argb >> 16) & 0xFF;
    uint32_t g = (argb >> 8) & 0xFF;
    uint32_t b = argb & 0xFF;

    r = (uint32_t) (r * mul);
    g = (uint32_t) (g * mul);
    b = (uint32_t) (b * mul);

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline void colorToFloats(uint32_t argb, float *r, float *g, float *b, float *a) {
    *a = ((argb >> 24) & 0xFF) / 255.0f;
    *r = ((argb >> 16) & 0xFF) / 255.0f;
    *g = ((argb >> 8) & 0xFF) / 255.0f;
    *b = (argb & 0xFF) / 255.0f;
}

Font::Font(const char *ttfPath, int pixelHeight)
    : m_shader("shaders/font.vert", "shaders/font.frag"), m_ft(nullptr), m_face(nullptr), m_screenProjection(Mat4::orthographic(0.0, 1920.0, 1080.0, 0.0, -1.0, 1.0)), m_worldView(Mat4::identity()), m_worldProjection(Mat4::identity()), m_vao(0), m_vbo(0),
      m_nearest(false), m_shadowOffsetX(1.0f), m_shadowOffsetY(1.0f), m_shadowOffsetZ(0.001f) {
    if (FT_Init_FreeType(&m_ft) != 0) throw std::runtime_error("FT_Init_FreeType failed");
    if (FT_New_Face(m_ft, ttfPath, 0, &m_face) != 0) {
        FT_Done_FreeType(m_ft);
        throw std::runtime_error("FT_New_Face failed (bad path/font?)");
    }

    FT_Set_Pixel_Sizes(m_face, 0, (FT_UInt) pixelHeight);

    m_vao = RenderCommand::createVertexArray();
    m_vbo = RenderCommand::createBuffer();

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    RenderCommand::uploadArrayBuffer(nullptr, (uint32_t) (6 * 5 * sizeof(float)), RC_DYNAMIC_DRAW);

    RenderCommand::enableVertexAttrib(0);
    RenderCommand::setVertexAttribPointer(0, 3, RC_FLOAT, false, 5 * sizeof(float), 0);

    RenderCommand::enableVertexAttrib(1);
    RenderCommand::setVertexAttribPointer(1, 2, RC_FLOAT, false, 5 * sizeof(float), 3 * sizeof(float));

    m_shader.bind();
    m_shader.setInt("u_font", 0);
}

Font::~Font() {
    for (auto &kv : m_glyphs)
        if (kv.second.texture) RenderCommand::deleteTexture(kv.second.texture);

    m_glyphs.clear();

    RenderCommand::deleteBuffer(m_vbo);
    RenderCommand::deleteVertexArray(m_vao);

    if (m_face) FT_Done_Face(m_face);
    if (m_ft) FT_Done_FreeType(m_ft);
}

void Font::setScreenProjection(const Mat4 &projection) { m_screenProjection = projection; }

void Font::setWorldViewProjection(const Mat4 &view, const Mat4 &projection) {
    m_worldView       = view;
    m_worldProjection = projection;
}

void Font::setNearest(bool nearest) {
    m_nearest = nearest;

    for (auto &kv : m_glyphs) {
        if (!kv.second.texture) continue;

        RenderCommand::bindTexture2D(kv.second.texture);

        uint32_t filter = m_nearest ? RC_NEAREST : RC_LINEAR;
        RenderCommand::setTextureParameteri(RC_TEXTURE_MIN_FILTER, filter);
        RenderCommand::setTextureParameteri(RC_TEXTURE_MAG_FILTER, filter);
        RenderCommand::setTextureParameteri(RC_TEXTURE_WRAP_S, RC_CLAMP_TO_EDGE);
        RenderCommand::setTextureParameteri(RC_TEXTURE_WRAP_T, RC_CLAMP_TO_EDGE);
    }
}

void Font::setShadowOffset(float x, float y, float z) {
    m_shadowOffsetX = x;
    m_shadowOffsetY = y;
    m_shadowOffsetZ = z;
}

float Font::getWidth(std::wstring_view text, float scale) const {
    if (text.empty()) return 0.0f;

    float width = 0.0f;

    for (wchar_t wc : text) {
        if (wc == L'\n') break;
        uint32_t cp = (uint32_t) wc;

        auto it            = m_glyphs.find(cp);
        const Glyph *glyph = nullptr;

        if (it != m_glyphs.end()) glyph = &it->second;
        else
            glyph = &const_cast<Font *>(this)->getOrCreateGlyph(cp);

        if (!glyph->texture) continue;
        width += (float) (glyph->advance >> 6) * scale;
    }

    return width;
}

void Font::draw(std::wstring_view text, float x, float y, float scale, uint32_t argb) {
    if (text.empty()) return;

    m_shader.setMat4("u_model", GlStateManager::getMatrix().data());

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
    m_shader.setMat4("u_view", Mat4::identity().data());
    m_shader.setMat4("u_projection", m_screenProjection.data());
    m_shader.setMat4("u_model", GlStateManager::getMatrix().data());
    m_shader.setVec3("u_color", r, g, b);
    m_shader.setFloat("u_alpha", a);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    float penX = x;
    float penY = y;

    for (wchar_t wc : text) {
        uint32_t cp = (uint32_t) wc;
        if (cp == (uint32_t) L'\n') {
            penX = x;
            penY += 32.0f * scale;
            continue;
        }

        const Glyph &ch = getOrCreateGlyph(cp);
        if (!ch.texture) continue;

        float xPos = penX + (float) ch.bearingX * scale;
        float yPos = penY - (float) ch.bearingY * scale;

        float width  = (float) ch.width * scale;
        float height = (float) ch.height * scale;

        float vertices[6][5] = {
                {xPos, yPos, 0.0f, 0.0f, 0.0f}, {xPos + width, yPos, 0.0f, 1.0f, 0.0f}, {xPos + width, yPos + height, 0.0f, 1.0f, 1.0f}, {xPos, yPos, 0.0f, 0.0f, 0.0f}, {xPos + width, yPos + height, 0.0f, 1.0f, 1.0f}, {xPos, yPos + height, 0.0f, 0.0f, 1.0f},
        };

        RenderCommand::activeTexture(0);
        RenderCommand::bindTexture2D(ch.texture);

        RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), RC_DYNAMIC_DRAW);
        RenderCommand::drawArrays(RC_TRIANGLES, 0, 6);

        penX += (float) (ch.advance >> 6) * scale;
    }

    GlStateManager::setDepthMask(true);
    GlStateManager::enableDepthTest();
}

void Font::drawShadow(std::wstring_view text, float x, float y, float scale, uint32_t argb) {
    uint32_t shadow = mulColor(argb, 0.25f);
    draw(text, x + m_shadowOffsetX * scale, y + m_shadowOffsetY * scale, scale, shadow);
    draw(text, x, y, scale, argb);
}

void Font::worldDraw(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb) {
    if (text.empty()) return;

    m_shader.setMat4("u_model", GlStateManager::getMatrix().data());

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
    m_shader.setMat4("u_view", m_worldView.data());
    m_shader.setMat4("u_projection", m_worldProjection.data());
    m_shader.setMat4("u_model", GlStateManager::getMatrix().data());
    m_shader.setVec3("u_color", r, g, b);
    m_shader.setFloat("u_alpha", a);

    RenderCommand::bindVertexArray(m_vao);
    RenderCommand::bindArrayBuffer(m_vbo);

    float penX = 0.0f;
    float penY = 0.0f;

    for (wchar_t wc : text) {
        uint32_t cp = (uint32_t) wc;
        if (cp == (uint32_t) L'\n') {
            penX = 0.0f;
            penY -= 32.0f * scale;
            continue;
        }

        const Glyph &ch = getOrCreateGlyph(cp);
        if (!ch.texture) continue;

        float xPos = penX + (float) ch.bearingX * scale;
        float yTop = penY + (float) ch.bearingY * scale;

        float width  = (float) ch.width * scale;
        float height = (float) ch.height * scale;

        float x0 = pos.x + xPos;
        float y0 = pos.y + yTop;
        float z0 = pos.z;

        float vertices[6][5] = {
                {x0, y0, z0, 0.0f, 0.0f}, {x0 + width, y0, z0, 1.0f, 0.0f}, {x0 + width, y0 - height, z0, 1.0f, 1.0f}, {x0, y0, z0, 0.0f, 0.0f}, {x0 + width, y0 - height, z0, 1.0f, 1.0f}, {x0, y0 - height, z0, 0.0f, 1.0f},
        };

        RenderCommand::activeTexture(0);
        RenderCommand::bindTexture2D(ch.texture);

        RenderCommand::uploadArrayBuffer(vertices, (uint32_t) sizeof(vertices), RC_DYNAMIC_DRAW);
        RenderCommand::drawArrays(RC_TRIANGLES, 0, 6);

        penX += (float) (ch.advance >> 6) * scale;
    }

    GlStateManager::setDepthMask(true);
    GlStateManager::enableDepthTest();
}

void Font::worldDrawShadow(std::wstring_view text, const Vec3 &pos, float scale, uint32_t argb) {
    uint32_t shadow = mulColor(argb, 0.25f);

    float ox = m_shadowOffsetX * scale;
    float oy = m_shadowOffsetY * scale;

    float oz = m_shadowOffsetZ;
    if (oz == 0.0f) oz = 0.01f;
    oz *= scale;

    worldDraw(text, Vec3(pos.x + ox, pos.y - oy, pos.z - oz), scale, shadow);
    worldDraw(text, pos, scale, argb);
}

const Font::Glyph &Font::getOrCreateGlyph(uint32_t codepoint) {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) return it->second;

    if (FT_Load_Char(m_face, codepoint, FT_LOAD_RENDER) != 0) {
        if (codepoint != (uint32_t) '?' && FT_Load_Char(m_face, (uint32_t) '?', FT_LOAD_RENDER) == 0) { return getOrCreateGlyph((uint32_t) '?'); }
        Glyph empty{};
        auto [insIt, _] = m_glyphs.emplace(codepoint, empty);
        return insIt->second;
    }

    FT_GlyphSlot glyphSlot = m_face->glyph;

    uint32_t texture = RenderCommand::createTexture();
    RenderCommand::bindTexture2D(texture);

    RenderCommand::pixelStorei(RC_UNPACK_ALIGNMENT, 1);

    uint32_t filter = m_nearest ? RC_NEAREST : RC_LINEAR;

    RenderCommand::setTextureParameteri(RC_TEXTURE_MIN_FILTER, filter);
    RenderCommand::setTextureParameteri(RC_TEXTURE_MAG_FILTER, filter);
    RenderCommand::setTextureParameteri(RC_TEXTURE_WRAP_S, RC_CLAMP_TO_EDGE);
    RenderCommand::setTextureParameteri(RC_TEXTURE_WRAP_T, RC_CLAMP_TO_EDGE);

    RenderCommand::uploadTexture2D((int) glyphSlot->bitmap.width, (int) glyphSlot->bitmap.rows, RC_RED, RC_RED, RC_UNSIGNED_BYTE, glyphSlot->bitmap.buffer);

    Glyph glyph;
    glyph.texture  = texture;
    glyph.width    = (int) glyphSlot->bitmap.width;
    glyph.height   = (int) glyphSlot->bitmap.rows;
    glyph.bearingX = (int) glyphSlot->bitmap_left;
    glyph.bearingY = (int) glyphSlot->bitmap_top;
    glyph.advance  = (uint32_t) glyphSlot->advance.x;

    auto [insertIt, _] = m_glyphs.emplace(codepoint, glyph);
    return insertIt->second;
}
