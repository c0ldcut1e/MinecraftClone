#include "UIControl_List.h"

#include <algorithm>
#include <memory>

#include "UIPanelProperties.h"

class UIPanelProperties_List : public UIPanelProperties
{
public:
    UIPanelProperties_List(float x, float y, float width, float height, float transparency)
        : UIPanelProperties(x, y, width, height, transparency)
    {}

    ~UIPanelProperties_List() override {}

    const char *getTexturePath() const override { return "textures/ui/panel9grid.png"; }
};

UIControl_List::UIControl_List(const std::string &name, float x, float y, float width, float height,
                               float transparency)
    : UIControl(name, x, y, width, height),
      m_panel(std::make_unique<UIComponent_Panel>(
              std::make_unique<UIPanelProperties_List>(x, y, width, height, transparency))),
      m_transparency(transparency), m_contentPaddingX(24.0f), m_contentPaddingY(24.0f)
{}

UIControl_List::~UIControl_List() {}

void UIControl_List::tick()
{
    syncPanel();

    for (UIControl *control : m_controls)
    {
        if (control)
        {
            control->setLayoutOrigin(m_x, m_y);
            control->tick();
        }
    }
}

void UIControl_List::render()
{
    syncPanel();

    if (m_panel)
    {
        m_panel->render();
    }

    for (UIControl *control : m_controls)
    {
        if (control)
        {
            control->setLayoutOrigin(m_x, m_y);
            control->render();
        }
    }
}

void UIControl_List::addControl(UIControl *control)
{
    if (!control)
    {
        return;
    }

    m_controls.push_back(control);
    control->setLayoutOrigin(m_x, m_y);
}

void UIControl_List::removeControl(UIControl *control)
{
    if (!control)
    {
        return;
    }

    control->clearLayoutOrigin();
    m_controls.erase(std::remove(m_controls.begin(), m_controls.end(), control), m_controls.end());
}

UIControl *UIControl_List::getControlById(uint32_t id) const
{
    for (UIControl *control : m_controls)
    {
        if (control && control->getId() == id)
        {
            return control;
        }
    }

    return nullptr;
}

void UIControl_List::resizeVertical(float padding)
{
    float maxBottom = m_y + m_contentPaddingY;
    for (UIControl *control : m_controls)
    {
        if (!control)
        {
            continue;
        }

        maxBottom = std::max(maxBottom, control->getY() + control->getPreferredHeight());
    }

    m_height = std::max(m_height, maxBottom - m_y + padding);
}

void UIControl_List::resizeHorizontal(float padding)
{
    float maxRight = m_x + m_contentPaddingX;
    for (UIControl *control : m_controls)
    {
        if (!control)
        {
            continue;
        }

        maxRight = std::max(maxRight, control->getX() + control->getPreferredWidth());
    }

    m_width = std::max(m_width, maxRight - m_x + padding);
}

void UIControl_List::syncPanel()
{
    if (!m_panel)
    {
        return;
    }

    UIPanelProperties *properties = m_panel->getProperties();
    if (!properties)
    {
        return;
    }

    properties->x            = m_x;
    properties->y            = m_y;
    properties->width        = m_width;
    properties->height       = m_height;
    properties->transparency = m_transparency;
}
