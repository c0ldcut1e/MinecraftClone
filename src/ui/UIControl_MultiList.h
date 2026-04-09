#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "UIControl_Checkbox.h"
#include "UIControl_Label.h"
#include "UIControl_List.h"
#include "UIControl_Slider.h"

class UIControl_MultiList : public UIControl_List
{
public:
    UIControl_MultiList(float x, float y, float width, float height, float transparency = 1.0f);
    ~UIControl_MultiList() override;

    void tick() override;

    UIControl_Checkbox *addNewCheckbox(uint32_t id, const std::wstring &label,
                                       bool checked = false);
    UIControl_Slider *addNewSlider(uint32_t id, const std::wstring &name, float minValue,
                                   float maxValue, float defaultValue, float increment);
    UIControl_Label *addNewLabel(uint32_t id, const std::wstring &text, uint32_t color = 0xFF484848,
                                 bool shadow = false);

    void setWrapSelection(bool wrapSelection);
    void setContentPadding(float contentPaddingX, float contentPaddingY);
    void setRowSpacing(float rowSpacing);
    void setCheckboxRowHeight(float checkboxRowHeight);
    void setSliderRowHeight(float sliderRowHeight);
    void setLabelRowHeight(float labelRowHeight);
    void setCheckboxUpdatedCallback(const std::function<void(uint32_t id, bool checked)> &callback);
    void setSliderUpdatedCallback(const std::function<void(uint32_t id, float value)> &callback);

protected:
    virtual void onCheckboxUpdated(uint32_t id, bool checked);
    virtual void onSliderUpdated(uint32_t id, float value);

private:
    enum class EntryType
    {
        Checkbox,
        Slider,
        Label,
    };

    struct Entry
    {
        uint32_t id;
        EntryType type;
        UIControl *control;
        bool selectable;
    };

    void layoutEntries();
    void updateSelectionFromMouse();
    void updateSelectionFromController();
    void syncSelectedStates();
    int findHoveredSelectableEntry(int mouseX, int mouseY, int actualWidth, int actualHeight) const;
    int findFirstSelectableEntry() const;
    int getNextSelectableEntry(int startIndex, int direction) const;
    float getEntryRowHeight(const Entry &entry) const;
    void activateEntry(int entryIndex);
    void stepSelectedEntry(int direction);
    void notifyCheckboxUpdated(uint32_t id, bool checked);
    void notifySliderUpdated(uint32_t id, float value);

    std::vector<Entry> m_entries;
    std::function<void(uint32_t id, bool checked)> m_checkboxUpdatedCallback;
    std::function<void(uint32_t id, float value)> m_sliderUpdatedCallback;
    int m_selectedIndex;
    int m_pointerActiveIndex;
    bool m_previousMousePrimaryDown;
    int m_previousMouseX;
    int m_previousMouseY;
    bool m_usingMouseSelection;
    bool m_wrapSelection;
    float m_rowSpacing;
    float m_checkboxRowHeight;
    float m_sliderRowHeight;
    float m_labelRowHeight;
};
