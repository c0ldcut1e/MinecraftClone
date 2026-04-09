#include "UIControl_MultiList.h"

#include <algorithm>

#include <SDL2/SDL.h>

#include "../core/Minecraft.h"
#include "../input/InputManager.h"
#include "../utils/math/Mth.h"

UIControl_MultiList::UIControl_MultiList(float x, float y, float width, float height,
                                         float transparency)
    : UIControl_List("Control_MultiList", x, y, width, height, transparency), m_entries(),
      m_checkboxUpdatedCallback(), m_sliderUpdatedCallback(), m_selectedIndex(-1),
      m_pointerActiveIndex(-1), m_previousMousePrimaryDown(false), m_previousMouseX(0),
      m_previousMouseY(0), m_usingMouseSelection(true), m_wrapSelection(true), m_rowSpacing(6.0f),
      m_checkboxRowHeight(28.0f), m_sliderRowHeight(36.0f), m_labelRowHeight(20.0f)
{
    m_contentPaddingX = 18.0f;
    m_contentPaddingY = 16.0f;
}

UIControl_MultiList::~UIControl_MultiList() {}

void UIControl_MultiList::tick()
{
    layoutEntries();
    updateSelectionFromController();
    updateSelectionFromMouse();
    syncSelectedStates();
    UIControl_List::tick();
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

void UIControl_MultiList::setWrapSelection(bool wrapSelection) { m_wrapSelection = wrapSelection; }

void UIControl_MultiList::setContentPadding(float contentPaddingX, float contentPaddingY)
{
    m_contentPaddingX = contentPaddingX;
    m_contentPaddingY = contentPaddingY;
}

void UIControl_MultiList::setRowSpacing(float rowSpacing) { m_rowSpacing = rowSpacing; }

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

void UIControl_MultiList::layoutEntries()
{
    float currentY = m_y + m_contentPaddingY;
    float rowWidth = m_width - m_contentPaddingX * 2.0f;

    for (const Entry &entry : m_entries)
    {
        if (!entry.control)
        {
            continue;
        }

        float rowHeight = getEntryRowHeight(entry);
        entry.control->setBounds(m_x + m_contentPaddingX, currentY, rowWidth, rowHeight);
        currentY += rowHeight + m_rowSpacing;
    }
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
        movedWithController = true;
        m_selectedIndex     = getNextSelectableEntry(m_selectedIndex, -1);
    }

    if (inputManager->consumeUiNavigateDownPress())
    {
        movedWithController = true;
        m_selectedIndex     = getNextSelectableEntry(m_selectedIndex, 1);
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
            m_entries[i].control->setSelected(m_entries[i].selectable &&
                                              (int) i == m_selectedIndex);
        }
    }
}

int UIControl_MultiList::findHoveredSelectableEntry(int mouseX, int mouseY, int actualWidth,
                                                    int actualHeight) const
{
    for (size_t i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].selectable || !m_entries[i].control)
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
        if (m_entries[i].selectable && m_entries[i].control)
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

        if (m_entries[(size_t) index].selectable && m_entries[(size_t) index].control)
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
    if (!entry.control)
    {
        return 0.0f;
    }

    if (entry.type == EntryType::Checkbox)
    {
        return std::max(entry.control->getPreferredHeight(), m_checkboxRowHeight);
    }
    if (entry.type == EntryType::Slider)
    {
        return std::max(entry.control->getPreferredHeight(), m_sliderRowHeight);
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
    if (!entry.control)
    {
        return;
    }

    entry.control->activate();
    if (entry.type == EntryType::Checkbox)
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
    if (!entry.control)
    {
        return;
    }

    if (entry.control->stepValue(direction) && entry.type == EntryType::Slider)
    {
        UIControl_Slider *slider = (UIControl_Slider *) entry.control;
        notifySliderUpdated(slider->getId(), slider->getValue());
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

void UIControl_MultiList::onCheckboxUpdated(uint32_t, bool) {}

void UIControl_MultiList::onSliderUpdated(uint32_t, float) {}
