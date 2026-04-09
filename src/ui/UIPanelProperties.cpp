#include "UIPanelProperties.h"

UIPanelProperties::UIPanelProperties(float x, float y, float width, float height,
                                     float transparency, void *customProperties)
    : x(x), y(y), width(width), height(height), transparency(transparency),
      customProperties(customProperties)
{}

UIPanelProperties::~UIPanelProperties() {}
