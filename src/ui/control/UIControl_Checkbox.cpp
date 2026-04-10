#include "UIControl_Checkbox.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glad/glad.h>

#include "../../core/Minecraft.h"
#include "../../rendering/BufferBuilder.h"
#include "../../rendering/Font.h"
#include "../../rendering/GlStateManager.h"
#include "../../rendering/Tesselator.h"
#include "../UIScreen.h"

#define CHECKBOX_VIRTUAL_SIZE   28.0f
#define CHECKBOX_LABEL_SCALE    0.85f
#define CHECKBOX_LABEL_SPACING  4.0f
#define CHECKBOX_LABEL_Y_OFFSET 1.5f

static void emitCheckboxQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1,
                             float u0, float v0, float u1, float v1, uint32_t color)
{
    if (!builder || x1 <= x0 || y1 <= y0)
    {
        return;
    }

    builder->color(color);
    builder->vertexUV(x0, y0, 0.0f, u0, v0);
    builder->vertexUV(x1, y0, 0.0f, u1, v0);
    builder->vertexUV(x1, y1, 0.0f, u1, v1);
    builder->vertexUV(x0, y0, 0.0f, u0, v0);
    builder->vertexUV(x1, y1, 0.0f, u1, v1);
    builder->vertexUV(x0, y1, 0.0f, u0, v1);
}

UIControl_Checkbox::UIControl_Checkbox(float x, float y, float width, float height,
                                       const std::wstring &label, bool checked)
    : UIControl("Control_Checkbox", x, y, width, height), m_label(label), m_checked(checked),
      m_backgroundTexture("textures/ui/tickbox_background.png"),
      m_overTexture("textures/ui/tickbox_over.png"), m_tickTexture("textures/ui/tick.png")
{}

UIControl_Checkbox::~UIControl_Checkbox() {}

void UIControl_Checkbox::render()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    int actualWidth  = minecraft->getWidth();
    int actualHeight = minecraft->getHeight();
    Font *font       = minecraft->getDefaultFont();
    if (!font)
    {
        return;
    }

    UIScreen::Rect controlRect = getActualRect(actualWidth, actualHeight);
    float uiScale              = UIScreen::scaleUniform(actualWidth, actualHeight);
    if (uiScale <= 0.0f)
    {
        return;
    }

    float boxSize =
            std::round(UIScreen::toActualLength(CHECKBOX_VIRTUAL_SIZE, actualWidth, actualHeight));
    float boxX = std::round(controlRect.x);
    float boxY = std::round(controlRect.y + (controlRect.height - boxSize) * 0.5f);

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();
    Texture *background    = m_selected ? &m_overTexture : &m_backgroundTexture;

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    builder->begin(GL_TRIANGLES);
    builder->bindTexture(background);
    builder->setAlphaCutoff(0.01f);
    emitCheckboxQuad(builder, boxX, boxY, boxX + boxSize, boxY + boxSize, 0.0f, 0.0f, 1.0f, 1.0f,
                     0xFFFFFFFFu);
    builder->end();
    builder->unbindTexture();

    if (m_checked)
    {
        float tickWidth = std::round(boxSize * ((float) m_tickTexture.getPixelWidth() /
                                                (float) m_tickTexture.getPixelHeight()));
        builder->begin(GL_TRIANGLES);
        builder->bindTexture(&m_tickTexture);
        builder->setAlphaCutoff(0.5f);
        emitCheckboxQuad(builder, boxX, boxY, boxX + tickWidth, boxY + boxSize, 0.0f, 0.0f, 1.0f,
                         1.0f, 0xFFFFFFFFu);
        builder->end();
        builder->unbindTexture();
    }

    builder->setAlphaCutoff(-1.0f);

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();

    float fontScale = font->snapScale(uiScale * CHECKBOX_LABEL_SCALE);
    float labelSpacing =
            std::round(UIScreen::toActualLength(CHECKBOX_LABEL_SPACING, actualWidth, actualHeight));
    float labelYOffset = std::round(
            UIScreen::toActualLength(CHECKBOX_LABEL_Y_OFFSET, actualWidth, actualHeight));
    float textX            = std::round(boxX + boxSize + labelSpacing);
    float lineBottomOffset = font->getLineHeight(fontScale) - font->getAscent(fontScale);
    float textBaselineY    = std::round(boxY + boxSize - lineBottomOffset - labelYOffset);
    font->setNearest(true);
    if (m_selected)
    {
        font->draw(m_label, textX + 1.0f, textBaselineY + 1.0f, fontScale, 0xFF000000);
    }
    font->draw(m_label, textX, textBaselineY, fontScale, m_selected ? 0xFFFFFF00 : 0xFF484848);
}

float UIControl_Checkbox::getPreferredWidth() const
{
    return CHECKBOX_VIRTUAL_SIZE + CHECKBOX_LABEL_SPACING + measureLabelWidth();
}

float UIControl_Checkbox::getPreferredHeight() const { return CHECKBOX_VIRTUAL_SIZE; }

void UIControl_Checkbox::resizeHorizontal(float) {}

void UIControl_Checkbox::resizeVertical(float) {}

void UIControl_Checkbox::activate() { m_checked = !m_checked; }

bool UIControl_Checkbox::onPointerPressed(int, int, int, int)
{
    activate();
    return true;
}

void UIControl_Checkbox::setChecked(bool checked) { m_checked = checked; }

bool UIControl_Checkbox::isChecked() const { return m_checked; }

float UIControl_Checkbox::measureLabelWidth() const
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return m_width;
    }

    Font *font = minecraft->getDefaultFont();
    if (!font)
    {
        return m_width;
    }

    return font->getWidth(m_label, 1.0f);
}
