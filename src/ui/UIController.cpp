#include "UIController.h"

UIController::UIController() {}

UIController::~UIController() {}

void UIController::tick() {
    for (UIScene *scene : m_sceneStack)
        if (scene) scene->tick();
}

void UIController::render() {
    for (UIScene *scene : m_sceneStack)
        if (scene) scene->render();
}

void UIController::pushScene(UIScene *scene) {
    if (!scene) return;
    scene->onEnter();
    m_sceneStack.push_back(scene);
}

void UIController::popScene() {
    if (m_sceneStack.empty()) return;
    UIScene *scene = m_sceneStack.back();
    m_sceneStack.pop_back();
    if (scene) scene->onExit();
}

UIScene *UIController::getTopScene() const {
    if (m_sceneStack.empty()) return nullptr;
    return m_sceneStack.back();
}
