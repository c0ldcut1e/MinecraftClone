#pragma once

#include <string>
#include <vector>

#include "UILayer.h"

class UIScene {
public:
    explicit UIScene(const std::string &name);
    virtual ~UIScene();

    virtual void tick();
    virtual void render();

    virtual void onEnter();
    virtual void onExit();

    void addLayer(UILayer *layer);
    void removeLayer(UILayer *layer);

    const std::string &getName() const;

private:
    std::string m_name;
    std::vector<UILayer *> m_layers;
};
