#pragma once

class UIPanelProperties
{
public:
    UIPanelProperties(float x, float y, float width, float height, float transparency = 1.0f,
                      void *customProperties = nullptr);
    virtual ~UIPanelProperties();

    virtual const char *getTexturePath() const = 0;

    float x;
    float y;
    float width;
    float height;
    float transparency;
    void *customProperties;
};
