#include "UIControl_MultiList.h"

#include <algorithm>

#include <SDL2/SDL.h>
#include <glad/glad.h>

#include "../../core/Minecraft.h"
#include "../../input/InputManager.h"
#include "../../rendering/BufferBuilder.h"
#include "../../rendering/GlStateManager.h"
#include "../../rendering/Tesselator.h"
#include "../../utils/math/Mth.h"

#define MULTILIST_SCROLL_ICON_RIGHT_PADDING     20.0f
#define MULTILIST_SCROLL_ICON_BOTTOM_PADDING    20.0f
#define MULTILIST_SCROLL_ICON_TOP_GAP           8.0f
#define MULTILIST_SCROLL_ICON_GAP               8.0f
#define MULTILIST_SCROLL_ICON_VIRTUAL_HEIGHT    25.0f
#define MULTILIST_SCROLL_ICON_FLASH_DURATION_MS 260
#define MULTILIST_SCROLL_ICON_FLASH_MIN_ALPHA   0.0f
#define MULTILIST_SCROLL_ICON_FLASH_OUT_RATIO   0.5f

static void emitMultiListQuad(BufferBuilder *builder, float x0, float y0, float x1, float y1,
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

static uint32_t packIndicatorColor(float alpha)
{
    uint32_t packedAlpha = (uint32_t) (Mth::clampf(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (packedAlpha << 24) | 0x00FFFFFF;
}

UIControl_MultiList::UIControl_MultiList(float x, float y, float width, float height,
                                         float transparency)
    : UIControl_List("Control_MultiList", x, y, width, height, transparency), m_entries(),
      m_buttonPressedCallback(), m_checkboxUpdatedCallback(), m_sliderUpdatedCallback(),
      m_selectedIndex(-1), m_pointerActiveIndex(-1), m_previousMousePrimaryDown(false),
      m_previousMouseX(0), m_previousMouseY(0), m_usingMouseSelection(true), m_wrapSelection(true),
      m_rowSpacing(6.0f), m_buttonRowHeight(40.0f), m_checkboxRowHeight(28.0f),
      m_sliderRowHeight(36.0f), m_labelRowHeight(20.0f), m_progressBarRowHeight(20.0f),
      m_maxVisibleElements(0), m_scrollMode(ScrollMode::Step), m_scrollOffset(0),
      m_scrollIndicatorFlashStartedAtMs(0),
      m_scrollIndicatorFlashDurationMs(MULTILIST_SCROLL_ICON_FLASH_DURATION_MS),
      m_scrollIndicatorFlashDirection(0), m_scrollUpTexture("textures/ui/scroll_up.png"),
      m_scrollDownTexture("textures/ui/scroll_down.png")
{
    m_contentPaddingX = 18.0f;
    m_contentPaddingY = 16.0f;
}

UIControl_MultiList::~UIControl_MultiList() {}

void UIControl_MultiList::tick()
{
    normalizeSelectionState();
    layoutEntries();
    updateSelectionFromController();
    updateSelectionFromMouse();
    normalizeSelectionState();
    layoutEntries();
    syncSelectedStates();
    UIControl_List::tick();
}

void UIControl_MultiList::render()
{
    UIControl_List::render();
    renderScrollIndicators();
}
UIControl_Button *UIControl_MultiList::addNewButton(uint32_t id, const std::wstring &label)
{
    UIControl_Button *button = new UIControl_Button(0.0f, 0.0f, 0.0f, m_buttonRowHeight, label);
    button->setId(id);
    addControl(button);
    m_entries.push_back({id, EntryType::Button, button, true});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return button;
}

UIControl_Checkbox *UIControl_MultiList::addNewCheckbox(uint32_t id, const std::wstring &label,
                                                        bool checked)
{
    UIControl_Checkbox *checkbox =
            new UIControl_Checkbox(0.0f, 0.0f, 0.0f, m_checkboxRowHeight, label, checked);
    checkbox->setId(id);
    addControl(checkbox);
    m_entries.push_back({id, EntryType::Checkbox, checkbox, true});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return checkbox;
}

UIControl_Slider *UIControl_MultiList::addNewSlider(uint32_t id, const std::wstring &name,
                                                    float minValue, float maxValue,
                                                    float defaultValue, float increment)
{
    UIControl_Slider *slider = new UIControl_Slider(0.0f, 0.0f, 0.0f, m_sliderRowHeight, name,
                                                    minValue, maxValue, defaultValue, increment);
    slider->setId(id);
    addControl(slider);
    m_entries.push_back({id, EntryType::Slider, slider, true});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return slider;
}

UIControl_Label *UIControl_MultiList::addNewLabel(uint32_t id, const std::wstring &text,
                                                  uint32_t color, bool shadow)
{
    UIControl_Label *label =
            new UIControl_Label(0.0f, 0.0f, 0.0f, m_labelRowHeight, text, color, shadow);
    label->setId(id);
    addControl(label);
    m_entries.push_back({id, EntryType::Label, label, false});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return label;
}

UIControl_ProgressBar *UIControl_MultiList::addNewProgressBar(uint32_t id, float progress)
{
    UIControl_ProgressBar *progressBar =
            new UIControl_ProgressBar(0.0f, 0.0f, 0.0f, m_progressBarRowHeight, progress);
    progressBar->setId(id);
    addControl(progressBar);
    m_entries.push_back({id, EntryType::ProgressBar, progressBar, false});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return progressBar;
}

UIControl_Spacer *UIControl_MultiList::addNewSpacer(uint32_t id, float height)
{
    UIControl_Spacer *spacer = new UIControl_Spacer(0.0f, 0.0f, 0.0f, height);
    spacer->setId(id);
    addControl(spacer);
    m_entries.push_back({id, EntryType::Spacer, spacer, false});
    if (m_selectedIndex < 0)
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    layoutEntries();
    syncSelectedStates();
    return spacer;
}

void UIControl_MultiList::setWrapSelection(bool wrapSelection) { m_wrapSelection = wrapSelection; }

void UIControl_MultiList::setContentPadding(float contentPaddingX, float contentPaddingY)
{
    m_contentPaddingX = contentPaddingX;
    m_contentPaddingY = contentPaddingY;
}

void UIControl_MultiList::setRowSpacing(float rowSpacing) { m_rowSpacing = rowSpacing; }

void UIControl_MultiList::setButtonRowHeight(float buttonRowHeight)
{
    m_buttonRowHeight = buttonRowHeight;
}

void UIControl_MultiList::setCheckboxRowHeight(float checkboxRowHeight)
{
    m_checkboxRowHeight = checkboxRowHeight;
}

void UIControl_MultiList::setSliderRowHeight(float sliderRowHeight)
{
    m_sliderRowHeight = sliderRowHeight;
}

void UIControl_MultiList::setLabelRowHeight(float labelRowHeight)
{
    m_labelRowHeight = labelRowHeight;
}

void UIControl_MultiList::setProgressBarRowHeight(float progressBarRowHeight)
{
    m_progressBarRowHeight = progressBarRowHeight;
}

void UIControl_MultiList::setMaxVisibleElements(int maxVisibleElements)
{
    m_maxVisibleElements = std::max(0, maxVisibleElements);
    m_scrollOffset       = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
    normalizeSelectionState();
}

int UIControl_MultiList::getMaxVisibleElements() const { return m_maxVisibleElements; }

void UIControl_MultiList::setScrollMode(ScrollMode scrollMode)
{
    m_scrollMode = scrollMode;
    normalizeSelectionState();
}

UIControl_MultiList::ScrollMode UIControl_MultiList::getScrollMode() const { return m_scrollMode; }

void UIControl_MultiList::setScrollIndicatorFlashDurationMs(uint32_t flashDurationMs)
{
    m_scrollIndicatorFlashDurationMs = flashDurationMs;
}

uint32_t UIControl_MultiList::getScrollIndicatorFlashDurationMs() const
{
    return m_scrollIndicatorFlashDurationMs;
}
void UIControl_MultiList::setButtonPressedCallback(const std::function<void(uint32_t id)> &callback)
{
    m_buttonPressedCallback = callback;
}

void UIControl_MultiList::setCheckboxUpdatedCallback(
        const std::function<void(uint32_t id, bool checked)> &callback)
{
    m_checkboxUpdatedCallback = callback;
}

void UIControl_MultiList::setSliderUpdatedCallback(
        const std::function<void(uint32_t id, float value)> &callback)
{
    m_sliderUpdatedCallback = callback;
}
void UIControl_MultiList::resizeVertical(float padding)
{
    if (m_entries.empty())
    {
        return;
    }

    float totalHeight = m_contentPaddingY;
    std::vector<float> visibleRowHeights;
    visibleRowHeights.reserve(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!isEntryVisible((int) i))
        {
            continue;
        }

        visibleRowHeights.push_back(getEntryRowHeight(m_entries[i]));
    }

    int rowsToShow = (int) visibleRowHeights.size();
    if (m_maxVisibleElements > 0)
    {
        rowsToShow = std::min(rowsToShow, m_maxVisibleElements);
    }

    float windowHeight = 0.0f;
    if (rowsToShow > 0)
    {
        int maxWindowStart = std::max(0, (int) visibleRowHeights.size() - rowsToShow);
        for (int start = 0; start <= maxWindowStart; start++)
        {
            float currentWindowHeight = 0.0f;
            for (int row = 0; row < rowsToShow; row++)
            {
                currentWindowHeight += visibleRowHeights[(size_t) (start + row)];
                if (row + 1 < rowsToShow)
                {
                    currentWindowHeight += m_rowSpacing;
                }
            }

            windowHeight = std::max(windowHeight, currentWindowHeight);
        }
    }

    float bottomSpace = std::max(padding, getScrollFooterHeight());
    totalHeight += windowHeight + bottomSpace;
    m_height = std::max(m_height, totalHeight);
}

void UIControl_MultiList::resizeHorizontal(float padding)
{
    float maxContentWidth = 0.0f;
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!isEntryVisible((int) i))
        {
            continue;
        }

        const Entry &entry = m_entries[i];
        if (entry.type == EntryType::Button || entry.type == EntryType::Slider ||
            entry.type == EntryType::ProgressBar)
        {
            maxContentWidth = std::max(maxContentWidth, entry.control->getPreferredWidth());
        }
    }

    if (maxContentWidth > 0.0f)
    {
        m_width = m_contentPaddingX * 2.0f + maxContentWidth + padding;
    }
}
bool UIControl_MultiList::shouldProcessControl(const UIControl *control) const
{
    const Entry *entry = findEntryForControl(control);
    if (!entry)
    {
        return UIControl_List::shouldProcessControl(control);
    }

    int entryIndex = (int) (entry - m_entries.data());
    return isEntryDisplayVisible(entryIndex);
}

void UIControl_MultiList::onButtonPressed(uint32_t) {}

void UIControl_MultiList::onCheckboxUpdated(uint32_t, bool) {}

void UIControl_MultiList::onSliderUpdated(uint32_t, float) {}
const UIControl_MultiList::Entry *
UIControl_MultiList::findEntryForControl(const UIControl *control) const
{
    if (!control)
    {
        return nullptr;
    }

    for (const Entry &entry : m_entries)
    {
        if (entry.control == control)
        {
            return &entry;
        }
    }

    return nullptr;
}

bool UIControl_MultiList::isEntryVisible(int entryIndex) const
{
    return entryIndex >= 0 && entryIndex < (int) m_entries.size() &&
           m_entries[(size_t) entryIndex].control &&
           m_entries[(size_t) entryIndex].control->isVisible();
}

bool UIControl_MultiList::isEntryDisplayVisible(int entryIndex) const
{
    if (!isEntryVisible(entryIndex))
    {
        return false;
    }

    if (m_maxVisibleElements <= 0)
    {
        return true;
    }

    int visibleOrdinal = getVisibleEntryOrdinal(entryIndex);
    if (visibleOrdinal < 0)
    {
        return false;
    }

    return visibleOrdinal >= m_scrollOffset &&
           visibleOrdinal < m_scrollOffset + m_maxVisibleElements;
}

int UIControl_MultiList::getVisibleEntryCount() const
{
    int visibleEntryCount = 0;
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (isEntryVisible((int) i))
        {
            visibleEntryCount++;
        }
    }

    return visibleEntryCount;
}

int UIControl_MultiList::getVisibleEntryOrdinal(int entryIndex) const
{
    if (!isEntryVisible(entryIndex))
    {
        return -1;
    }

    int visibleOrdinal = 0;
    for (int i = 0; i < entryIndex; i++)
    {
        if (isEntryVisible(i))
        {
            visibleOrdinal++;
        }
    }

    return visibleOrdinal;
}

int UIControl_MultiList::getMaxScrollOffset() const
{
    if (m_maxVisibleElements <= 0)
    {
        return 0;
    }

    return std::max(0, getVisibleEntryCount() - m_maxVisibleElements);
}

bool UIControl_MultiList::canScrollUp() const { return m_scrollOffset > 0; }

bool UIControl_MultiList::canScrollDown() const { return m_scrollOffset < getMaxScrollOffset(); }

float UIControl_MultiList::getScrollFooterHeight() const
{
    return m_maxVisibleElements > 0 && getVisibleEntryCount() > m_maxVisibleElements
                   ? MULTILIST_SCROLL_ICON_TOP_GAP + MULTILIST_SCROLL_ICON_VIRTUAL_HEIGHT +
                             MULTILIST_SCROLL_ICON_BOTTOM_PADDING
                   : 0.0f;
}

bool UIControl_MultiList::setScrollOffset(int scrollOffset, int direction)
{
    int clampedScrollOffset = std::clamp(scrollOffset, 0, getMaxScrollOffset());
    if (m_scrollOffset == clampedScrollOffset)
    {
        return false;
    }

    m_scrollOffset = clampedScrollOffset;
    if (direction != 0)
    {
        m_scrollIndicatorFlashDirection   = direction < 0 ? -1 : 1;
        m_scrollIndicatorFlashStartedAtMs = SDL_GetTicks();
    }

    return true;
}

bool UIControl_MultiList::scrollBy(int delta)
{
    return delta != 0 && setScrollOffset(m_scrollOffset + delta, delta);
}

void UIControl_MultiList::ensureSelectedEntryVisible()
{
    if (m_maxVisibleElements <= 0 || m_selectedIndex < 0 ||
        m_selectedIndex >= (int) m_entries.size())
    {
        m_scrollOffset = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
        return;
    }

    int visibleOrdinal = getVisibleEntryOrdinal(m_selectedIndex);
    if (visibleOrdinal < 0)
    {
        return;
    }

    if (visibleOrdinal < m_scrollOffset)
    {
        int targetScrollOffset = visibleOrdinal;
        if (m_scrollMode == ScrollMode::PageBySelection && m_maxVisibleElements > 0)
        {
            targetScrollOffset = std::max(0, visibleOrdinal - m_maxVisibleElements + 1);
        }

        setScrollOffset(targetScrollOffset, -1);
    }
    else if (visibleOrdinal >= m_scrollOffset + m_maxVisibleElements)
    {
        int targetScrollOffset = visibleOrdinal - m_maxVisibleElements + 1;
        if (m_scrollMode == ScrollMode::PageBySelection && m_maxVisibleElements > 0)
        {
            targetScrollOffset = visibleOrdinal;
        }

        setScrollOffset(targetScrollOffset, 1);
    }
    else
    {
        m_scrollOffset = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
    }
}

float UIControl_MultiList::getScrollIndicatorAlpha(int direction) const
{
    if (direction == 0 || m_scrollIndicatorFlashDirection != direction ||
        m_scrollIndicatorFlashDurationMs == 0)
    {
        return 1.0f;
    }

    uint32_t elapsedMs = SDL_GetTicks() - m_scrollIndicatorFlashStartedAtMs;
    if (elapsedMs >= m_scrollIndicatorFlashDurationMs)
    {
        return 1.0f;
    }

    float t = (float) elapsedMs / (float) m_scrollIndicatorFlashDurationMs;
    if (t < MULTILIST_SCROLL_ICON_FLASH_OUT_RATIO)
    {
        float outT = t / std::max(MULTILIST_SCROLL_ICON_FLASH_OUT_RATIO, 0.0001f);
        return Mth::lerpf(1.0f, MULTILIST_SCROLL_ICON_FLASH_MIN_ALPHA, outT);
    }

    float inT = (t - MULTILIST_SCROLL_ICON_FLASH_OUT_RATIO) /
                std::max(1.0f - MULTILIST_SCROLL_ICON_FLASH_OUT_RATIO, 0.0001f);
    return Mth::lerpf(MULTILIST_SCROLL_ICON_FLASH_MIN_ALPHA, 1.0f, inT);
}

void UIControl_MultiList::renderScrollIndicators()
{
    if (!isPanelVisible() || getScrollFooterHeight() <= 0.0f)
    {
        return;
    }

    bool showUp   = canScrollUp();
    bool showDown = canScrollDown();
    if (!showUp && !showDown)
    {
        return;
    }

    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    int actualWidth     = minecraft->getWidth();
    int actualHeight    = minecraft->getHeight();
    UIScreen::Rect rect = getActualRect(actualWidth, actualHeight);

    float rightPadding  = std::round(UIScreen::toActualLength(MULTILIST_SCROLL_ICON_RIGHT_PADDING,
                                                              actualWidth, actualHeight));
    float bottomPadding = std::round(UIScreen::toActualLength(MULTILIST_SCROLL_ICON_BOTTOM_PADDING,
                                                              actualWidth, actualHeight));
    float gap           = std::round(
            UIScreen::toActualLength(MULTILIST_SCROLL_ICON_GAP, actualWidth, actualHeight));
    float iconHeight = std::round(UIScreen::toActualLength(MULTILIST_SCROLL_ICON_VIRTUAL_HEIGHT,
                                                           actualWidth, actualHeight));
    float upIconWidth =
            std::round(iconHeight * ((float) std::max(m_scrollUpTexture.getPixelWidth(), 1) /
                                     (float) std::max(m_scrollUpTexture.getPixelHeight(), 1)));
    float downIconWidth =
            std::round(iconHeight * ((float) std::max(m_scrollDownTexture.getPixelWidth(), 1) /
                                     (float) std::max(m_scrollDownTexture.getPixelHeight(), 1)));
    float downX = std::round(rect.x + rect.width - rightPadding - downIconWidth);
    float upX   = std::round(downX - gap - upIconWidth);
    float iconY = std::round(rect.y + rect.height - bottomPadding - iconHeight);

    BufferBuilder *builder = Tesselator::getInstance()->getBuilderForScreen();

    GlStateManager::disableDepthTest();
    GlStateManager::disableCull();
    GlStateManager::enableBlend();
    GlStateManager::setBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (showUp)
    {
        builder->begin(GL_TRIANGLES);
        builder->bindTexture(&m_scrollUpTexture);
        builder->setAlphaCutoff(0.01f);
        emitMultiListQuad(builder, upX, iconY, upX + upIconWidth, iconY + iconHeight, 0.0f, 0.0f,
                          1.0f, 1.0f, packIndicatorColor(getScrollIndicatorAlpha(-1)));
        builder->end();
        builder->unbindTexture();
    }

    if (showDown)
    {
        builder->begin(GL_TRIANGLES);
        builder->bindTexture(&m_scrollDownTexture);
        builder->setAlphaCutoff(0.01f);
        emitMultiListQuad(builder, downX, iconY, downX + downIconWidth, iconY + iconHeight, 0.0f,
                          0.0f, 1.0f, 1.0f, packIndicatorColor(getScrollIndicatorAlpha(1)));
        builder->end();
        builder->unbindTexture();
    }

    builder->setAlphaCutoff(-1.0f);

    GlStateManager::disableBlend();
    GlStateManager::enableCull();
    GlStateManager::enableDepthTest();
}
void UIControl_MultiList::layoutEntries()
{
    float currentY = m_y + m_contentPaddingY;
    float rowWidth = m_width - m_contentPaddingX * 2.0f;

    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!isEntryDisplayVisible((int) i))
        {
            continue;
        }

        Entry &entry       = m_entries[i];
        float rowHeight    = getEntryRowHeight(entry);
        float displayWidth = entry.type == EntryType::ProgressBar
                                     ? std::max(rowWidth, entry.control->getPreferredWidth())
                                     : rowWidth;
        entry.control->setBounds(m_x + m_contentPaddingX, currentY, displayWidth, rowHeight);
        entry.control->resizeHorizontal(displayWidth);
        entry.control->resizeVertical(rowHeight);

        bool hasNextVisibleEntry = false;
        for (size_t j = i + 1; j < m_entries.size(); j++)
        {
            if (isEntryDisplayVisible((int) j))
            {
                hasNextVisibleEntry = true;
                break;
            }
        }

        currentY += rowHeight;
        if (hasNextVisibleEntry)
        {
            currentY += m_rowSpacing;
        }
    }
}

void UIControl_MultiList::normalizeSelectionState()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < (int) m_entries.size())
    {
        const Entry &entry = m_entries[(size_t) m_selectedIndex];
        if (!entry.selectable || !isEntryVisible(m_selectedIndex))
        {
            m_selectedIndex = findFirstSelectableEntry();
        }
    }
    else
    {
        m_selectedIndex = findFirstSelectableEntry();
    }

    if (m_pointerActiveIndex >= 0 && m_pointerActiveIndex < (int) m_entries.size())
    {
        if (!isEntryDisplayVisible(m_pointerActiveIndex))
        {
            m_pointerActiveIndex = -1;
        }
    }
    else
    {
        m_pointerActiveIndex = -1;
    }

    m_scrollOffset = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
    ensureSelectedEntryVisible();
}

void UIControl_MultiList::updateSelectionFromMouse()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    int mouseX            = 0;
    int mouseY            = 0;
    uint32_t mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
    bool mousePrimaryDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    bool mouseMoved       = mouseX != m_previousMouseX || mouseY != m_previousMouseY;

    if (mouseMoved)
    {
        m_usingMouseSelection = true;
    }

    int hoveredIndex = findHoveredSelectableEntry(mouseX, mouseY, minecraft->getWidth(),
                                                  minecraft->getHeight());

    if (m_pointerActiveIndex >= 0 && mousePrimaryDown &&
        m_pointerActiveIndex < (int) m_entries.size())
    {
        UIControl *activeControl = m_entries[(size_t) m_pointerActiveIndex].control;
        if (activeControl)
        {
            bool changed = activeControl->onPointerMoved(mouseX, mouseY, minecraft->getWidth(),
                                                         minecraft->getHeight());
            if (changed)
            {
                if (m_entries[(size_t) m_pointerActiveIndex].type == EntryType::Checkbox)
                {
                    UIControl_Checkbox *checkbox = static_cast<UIControl_Checkbox *>(
                            m_entries[(size_t) m_pointerActiveIndex].control);
                    notifyCheckboxUpdated(checkbox->getId(), checkbox->isChecked());
                }
                if (m_entries[(size_t) m_pointerActiveIndex].type == EntryType::Slider)
                {
                    UIControl_Slider *slider = static_cast<UIControl_Slider *>(
                            m_entries[(size_t) m_pointerActiveIndex].control);
                    notifySliderUpdated(slider->getId(), slider->getValue());
                }
            }
        }
    }

    if (m_usingMouseSelection && hoveredIndex >= 0 && m_pointerActiveIndex < 0)
    {
        m_selectedIndex = hoveredIndex;
    }

    if (hoveredIndex >= 0 && mousePrimaryDown && !m_previousMousePrimaryDown)
    {
        m_selectedIndex       = hoveredIndex;
        m_usingMouseSelection = true;
        m_pointerActiveIndex  = hoveredIndex;

        UIControl *control = m_entries[(size_t) hoveredIndex].control;
        if (control)
        {
            bool changed = control->onPointerPressed(mouseX, mouseY, minecraft->getWidth(),
                                                     minecraft->getHeight());
            if (changed)
            {
                if (m_entries[(size_t) hoveredIndex].type == EntryType::Button)
                {
                    notifyButtonPressed(m_entries[(size_t) hoveredIndex].id);
                }
                if (m_entries[(size_t) hoveredIndex].type == EntryType::Checkbox)
                {
                    UIControl_Checkbox *checkbox = static_cast<UIControl_Checkbox *>(
                            m_entries[(size_t) hoveredIndex].control);
                    notifyCheckboxUpdated(checkbox->getId(), checkbox->isChecked());
                }
                if (m_entries[(size_t) hoveredIndex].type == EntryType::Slider)
                {
                    UIControl_Slider *slider = static_cast<UIControl_Slider *>(
                            m_entries[(size_t) hoveredIndex].control);
                    notifySliderUpdated(slider->getId(), slider->getValue());
                }
            }
        }
    }

    if (!mousePrimaryDown && m_previousMousePrimaryDown && m_pointerActiveIndex >= 0 &&
        m_pointerActiveIndex < (int) m_entries.size())
    {
        UIControl *control = m_entries[(size_t) m_pointerActiveIndex].control;
        if (control)
        {
            bool changed = control->onPointerReleased(mouseX, mouseY, minecraft->getWidth(),
                                                      minecraft->getHeight());
            if (changed && m_entries[(size_t) m_pointerActiveIndex].type == EntryType::Slider)
            {
                UIControl_Slider *slider = static_cast<UIControl_Slider *>(
                        m_entries[(size_t) m_pointerActiveIndex].control);
                notifySliderUpdated(slider->getId(), slider->getValue());
            }
        }

        m_pointerActiveIndex = -1;
    }

    m_previousMousePrimaryDown = mousePrimaryDown;
    m_previousMouseX           = mouseX;
    m_previousMouseY           = mouseY;
}

void UIControl_MultiList::updateSelectionFromController()
{
    Minecraft *minecraft = Minecraft::getInstance();
    if (!minecraft)
    {
        return;
    }

    InputManager *inputManager = minecraft->getInputManager();
    if (!inputManager || m_entries.empty())
    {
        return;
    }

    bool movedWithController = false;
    if (inputManager->consumeUiNavigateUpPress())
    {
        movedWithController   = true;
        int nextSelectedIndex = getNextSelectableEntry(m_selectedIndex, -1);
        bool wrappedSelection = m_selectedIndex >= 0 && nextSelectedIndex >= 0 &&
                                nextSelectedIndex > m_selectedIndex;
        if ((nextSelectedIndex == m_selectedIndex || wrappedSelection) && canScrollUp())
        {
            scrollBy(-1);
        }
        else
        {
            m_selectedIndex = nextSelectedIndex;
            ensureSelectedEntryVisible();
        }
    }

    if (inputManager->consumeUiNavigateDownPress())
    {
        movedWithController   = true;
        int nextSelectedIndex = getNextSelectableEntry(m_selectedIndex, 1);
        bool wrappedSelection = m_selectedIndex >= 0 && nextSelectedIndex >= 0 &&
                                nextSelectedIndex < m_selectedIndex;
        if ((nextSelectedIndex == m_selectedIndex || wrappedSelection) && canScrollDown())
        {
            scrollBy(1);
        }
        else
        {
            m_selectedIndex = nextSelectedIndex;
            ensureSelectedEntryVisible();
        }
    }

    if (inputManager->consumeUiNavigateLeftPress())
    {
        movedWithController = true;
        stepSelectedEntry(-1);
    }

    if (inputManager->consumeUiNavigateRightPress())
    {
        movedWithController = true;
        stepSelectedEntry(1);
    }

    if (movedWithController)
    {
        m_usingMouseSelection = false;
    }

    if (inputManager->consumeUiActivatePress())
    {
        m_usingMouseSelection = false;
        activateEntry(m_selectedIndex);
    }
}

void UIControl_MultiList::syncSelectedStates()
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].control)
        {
            m_entries[i].control->setSelected(isEntryDisplayVisible((int) i) &&
                                              m_entries[i].selectable &&
                                              (int) i == m_selectedIndex);
        }
    }
}

int UIControl_MultiList::findHoveredSelectableEntry(int mouseX, int mouseY, int actualWidth,
                                                    int actualHeight) const
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].selectable || !isEntryDisplayVisible((int) i))
        {
            continue;
        }

        if (m_entries[i].control->containsActualPoint(mouseX, mouseY, actualWidth, actualHeight))
        {
            return (int) i;
        }
    }

    return -1;
}

int UIControl_MultiList::findFirstSelectableEntry() const
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].selectable && isEntryVisible((int) i))
        {
            return (int) i;
        }
    }

    return -1;
}

int UIControl_MultiList::getNextSelectableEntry(int startIndex, int direction) const
{
    if (m_entries.empty())
    {
        return -1;
    }

    if (startIndex < 0)
    {
        return findFirstSelectableEntry();
    }

    int entryCount = (int) m_entries.size();
    int index      = startIndex;
    for (int i = 0; i < entryCount; i++)
    {
        index += direction < 0 ? -1 : 1;
        if (m_wrapSelection)
        {
            if (index < 0)
            {
                index = entryCount - 1;
            }
            if (index >= entryCount)
            {
                index = 0;
            }
        }
        else
        {
            index = Mth::clampf(index, 0, entryCount - 1);
        }

        if (m_entries[(size_t) index].selectable && isEntryVisible(index))
        {
            return index;
        }

        if (!m_wrapSelection && (index == 0 || index == entryCount - 1))
        {
            break;
        }
    }

    return startIndex;
}

float UIControl_MultiList::getEntryRowHeight(const Entry &entry) const
{
    if (!entry.control || !entry.control->isVisible())
    {
        return 0.0f;
    }

    if (entry.type == EntryType::Button)
    {
        return std::max(entry.control->getPreferredHeight(), m_buttonRowHeight);
    }
    if (entry.type == EntryType::Checkbox)
    {
        return std::max(entry.control->getPreferredHeight(), m_checkboxRowHeight);
    }
    if (entry.type == EntryType::Slider)
    {
        return std::max(entry.control->getPreferredHeight(), m_sliderRowHeight);
    }
    if (entry.type == EntryType::ProgressBar)
    {
        return std::max(entry.control->getPreferredHeight(), m_progressBarRowHeight);
    }
    if (entry.type == EntryType::Spacer)
    {
        return std::max(entry.control->getPreferredHeight(), 0.0f);
    }

    return std::max(entry.control->getPreferredHeight(), m_labelRowHeight);
}

void UIControl_MultiList::activateEntry(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= (int) m_entries.size())
    {
        return;
    }

    Entry &entry = m_entries[(size_t) entryIndex];
    if (!entry.selectable || !isEntryDisplayVisible(entryIndex))
    {
        return;
    }

    entry.control->activate();
    if (entry.type == EntryType::Button)
    {
        notifyButtonPressed(entry.id);
    }
    else if (entry.type == EntryType::Checkbox)
    {
        UIControl_Checkbox *checkbox = (UIControl_Checkbox *) entry.control;
        notifyCheckboxUpdated(checkbox->getId(), checkbox->isChecked());
    }
}

void UIControl_MultiList::stepSelectedEntry(int direction)
{
    if (m_selectedIndex < 0 || m_selectedIndex >= (int) m_entries.size())
    {
        return;
    }

    Entry &entry = m_entries[(size_t) m_selectedIndex];
    if (!entry.selectable || !isEntryDisplayVisible(m_selectedIndex))
    {
        return;
    }

    if (entry.control->stepValue(direction) && entry.type == EntryType::Slider)
    {
        UIControl_Slider *slider = (UIControl_Slider *) entry.control;
        notifySliderUpdated(slider->getId(), slider->getValue());
    }
}

void UIControl_MultiList::notifyButtonPressed(uint32_t id)
{
    onButtonPressed(id);
    if (m_buttonPressedCallback)
    {
        m_buttonPressedCallback(id);
    }
}

void UIControl_MultiList::notifyCheckboxUpdated(uint32_t id, bool checked)
{
    onCheckboxUpdated(id, checked);
    if (m_checkboxUpdatedCallback)
    {
        m_checkboxUpdatedCallback(id, checked);
    }
}

void UIControl_MultiList::notifySliderUpdated(uint32_t id, float value)
{
    onSliderUpdated(id, value);
    if (m_sliderUpdatedCallback)
    {
        m_sliderUpdatedCallback(id, value);
    }
}

float UIControl_MultiList::getTypeRowHeight(EntryType type) const
{
    switch (type)
    {
        case EntryType::Button:
            return m_buttonRowHeight;
        case EntryType::Checkbox:
            return m_checkboxRowHeight;
        case EntryType::Slider:
            return m_sliderRowHeight;
        case EntryType::Label:
            return m_labelRowHeight;
        case EntryType::ProgressBar:
            return m_progressBarRowHeight;
        case EntryType::Spacer:
            return 0.0f;
        default:
            return 0.0f;
    }
}
