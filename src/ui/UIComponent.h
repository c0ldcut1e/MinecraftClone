#pragma once

#include <string>

class UIComponent {
public:
    explicit UIComponent(const std::string &name);
    virtual ~UIComponent();

    virtual void tick();
    virtual void render();

    const std::string &getName() const;

private:
    std::string m_name;
};
