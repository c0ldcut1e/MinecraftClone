#include "UIScene.h"

#include <algorithm>

UIScene::UIScene(const std::string &name) : m_name(name), m_visible(true) {}

UIScene::~UIScene() {}

void UIScene::tick()
{
    if (!m_visible)
    {
        return;
    }

    for (UILayer *layer : m_layers)
    {
        if (layer)
        {
            layer->tick();
        }
    }
}

void UIScene::render()
{
    if (!m_visible)
    {
        return;
    }

    for (UILayer *layer : m_layers)
    {
        if (layer)
        {
            layer->render();
        }
    }
}

void UIScene::onEnter() {}

void UIScene::onExit() {}

void UIScene::addLayer(UILayer *layer)
{
    if (!layer)
    {
        return;
    }

    m_layers.push_back(layer);
}

void UIScene::removeLayer(UILayer *layer)
{
    if (!layer)
    {
        return;
    }

    m_layers.erase(std::remove(m_layers.begin(), m_layers.end(), layer), m_layers.end());
}

void UIScene::setVisible(bool visible) { m_visible = visible; }

void UIScene::toggleVisible() { m_visible = !m_visible; }

bool UIScene::isVisible() const { return m_visible; }

const std::string &UIScene::getName() const { return m_name; }
