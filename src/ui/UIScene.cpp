#include "UIScene.h"

#include <algorithm>

UIScene::UIScene(const std::string &name) : m_name(name) {}

UIScene::~UIScene() {}

void UIScene::tick() {
    for (UILayer *layer : m_layers)
        if (layer) layer->tick();
}

void UIScene::render() {
    for (UILayer *layer : m_layers)
        if (layer) layer->render();
}

void UIScene::onEnter() {}

void UIScene::onExit() {}

void UIScene::addLayer(UILayer *layer) {
    if (!layer) return;
    m_layers.push_back(layer);
}

void UIScene::removeLayer(UILayer *layer) {
    if (!layer) return;
    m_layers.erase(std::remove(m_layers.begin(), m_layers.end(), layer), m_layers.end());
}

const std::string &UIScene::getName() const { return m_name; }
