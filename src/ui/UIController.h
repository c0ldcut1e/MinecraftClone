#pragma once

#include <vector>

#include "UIScene.h"

class UIController {
public:
    UIController();
    ~UIController();

    void tick();
    void render();

    void pushScene(UIScene *scene);
    void popScene();

    UIScene *getTopScene() const;

private:
    std::vector<UIScene *> m_sceneStack;
};
