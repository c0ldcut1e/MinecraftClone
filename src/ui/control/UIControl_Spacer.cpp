#include "UIControl_Spacer.h"

#include <algorithm>

UIControl_Spacer::UIControl_Spacer(float x, float y, float width, float height)
    : UIControl("Control_Spacer", x, y, width, height)
{}

UIControl_Spacer::~UIControl_Spacer() {}

void UIControl_Spacer::render() {}

float UIControl_Spacer::getPreferredWidth() const { return 0.0f; }

float UIControl_Spacer::getPreferredHeight() const { return std::max(m_height, 0.0f); }

bool UIControl_Spacer::isSelectable() const { return false; }
