#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "../../rendering/Texture.h"
#include "UIControl_Button.h"
#include "UIControl_Checkbox.h"
#include "UIControl_Label.h"
#include "UIControl_List.h"
#include "UIControl_ProgressBar.h"
#include "UIControl_Slider.h"
#include "UIControl_Spacer.h"

class UIControl_MultiList : public UIControl_List
{
public:
    enum class ScrollMode
    {
        Step,
        PageBySelection,
    };

    UIControl_MultiList(float x, float y, float width, float height, float transparency = 1.0f);
    ~UIControl_MultiList() override;

    void tick() override;
    void render() override;

    UIControl_Button *addNewButton(uint32_t id, const std::wstring &label);
    UIControl_Checkbox *addNewCheckbox(uint32_t id, const std::wstring &label,
                                       bool checked = false);
    UIControl_Slider *addNewSlider(uint32_t id, const std::wstring &name, float minValue,
                                   float maxValue, float defaultValue, float increment);
    UIControl_Label *addNewLabel(uint32_t id, const std::wstring &text, uint32_t color = 0xFF484848,
                                 bool shadow = false);
    UIControl_ProgressBar *addNewProgressBar(uint32_t id, float progress);
    UIControl_Spacer *addNewSpacer(uint32_t id, float height);

    void setWrapSelection(bool wrapSelection);
    void setContentPadding(float contentPaddingX, float contentPaddingY);
    void setRowSpacing(float rowSpacing);
    void setButtonRowHeight(float buttonRowHeight);
    void setCheckboxRowHeight(float checkboxRowHeight);
    void setSliderRowHeight(float sliderRowHeight);
    void setLabelRowHeight(float labelRowHeight);
    void setProgressBarRowHeight(float progressBarRowHeight);
    void setMaxVisibleElements(int maxVisibleElements);
    int getMaxVisibleElements() const;
    void setScrollMode(ScrollMode scrollMode);
    ScrollMode getScrollMode() const;
    void setScrollIndicatorFlashDurationMs(uint32_t flashDurationMs);
    uint32_t getScrollIndicatorFlashDurationMs() const;
    void setButtonPressedCallback(const std::function<void(uint32_t id)> &callback);
    void setCheckboxUpdatedCallback(const std::function<void(uint32_t id, bool checked)> &callback);
    void setSliderUpdatedCallback(const std::function<void(uint32_t id, float value)> &callback);

    void resizeVertical(float padding = 24.0f) override;
    void resizeHorizontal(float padding = 0.0f) override;

protected:
    bool shouldProcessControl(const UIControl *control) const override;
    virtual void onButtonPressed(uint32_t id);
    virtual void onCheckboxUpdated(uint32_t id, bool checked);
    virtual void onSliderUpdated(uint32_t id, float value);

private:
    enum class EntryType
    {
        Button,
        Checkbox,
        Slider,
        Label,
        ProgressBar,
        Spacer,
    };

    struct Entry
    {
        uint32_t id;
        EntryType type;
        UIControl *control;
        bool selectable;
    };

    const Entry *findEntryForControl(const UIControl *control) const;
    bool isEntryVisible(int entryIndex) const;
    bool isEntryDisplayVisible(int entryIndex) const;
    int getVisibleEntryCount() const;
    int getVisibleEntryOrdinal(int entryIndex) const;
    int getMaxScrollOffset() const;
    bool canScrollUp() const;
    bool canScrollDown() const;
    float getScrollFooterHeight() const;
    bool setScrollOffset(int scrollOffset, int direction);
    bool scrollBy(int delta);
    void ensureSelectedEntryVisible();
    float getScrollIndicatorAlpha(int direction) const;
    void renderScrollIndicators();
    void layoutEntries();
    void normalizeSelectionState();
    void updateSelectionFromMouse();
    void updateSelectionFromController();
    void syncSelectedStates();
    int findHoveredSelectableEntry(int mouseX, int mouseY, int actualWidth, int actualHeight) const;
    int findFirstSelectableEntry() const;
    int getNextSelectableEntry(int startIndex, int direction) const;
    float getEntryRowHeight(const Entry &entry) const;
    void activateEntry(int entryIndex);
    void stepSelectedEntry(int direction);
    void notifyButtonPressed(uint32_t id);
    void notifyCheckboxUpdated(uint32_t id, bool checked);
    void notifySliderUpdated(uint32_t id, float value);
    float getTypeRowHeight(EntryType type) const;

    std::vector<Entry> m_entries;
    std::function<void(uint32_t id)> m_buttonPressedCallback;
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
    float m_buttonRowHeight;
    float m_checkboxRowHeight;
    float m_sliderRowHeight;
    float m_labelRowHeight;
    float m_progressBarRowHeight;
    int m_maxVisibleElements;
    ScrollMode m_scrollMode;
    int m_scrollOffset;
    uint32_t m_scrollIndicatorFlashStartedAtMs;
    uint32_t m_scrollIndicatorFlashDurationMs;
    int m_scrollIndicatorFlashDirection;
    Texture m_scrollUpTexture;
    Texture m_scrollDownTexture;
};
