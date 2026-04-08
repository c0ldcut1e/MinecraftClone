#pragma once

class ImGuiSystem
{
public:
    ImGuiSystem();
    ~ImGuiSystem();

    void init(void *windowHandle);
    void shutdown();

    void beginFrame();
    void render();

    void setVisible(bool visible);
    void toggleVisible();
    bool isVisible() const;

    bool wantsKeyboardCapture() const;
    bool wantsMouseCapture() const;

private:
    bool m_initialized;
    bool m_visible;
};
