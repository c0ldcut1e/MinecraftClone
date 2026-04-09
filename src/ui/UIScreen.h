#pragma once

#include <algorithm>
#include <cmath>

#include "../utils/math/Mat4.h"

namespace UIScreen
{
    constexpr float WIDTH  = 1920.0f;
    constexpr float HEIGHT = 1080.0f;

    struct Rect
    {
        float x;
        float y;
        float width;
        float height;
    };

    inline Mat4 buildProjection()
    {
        return Mat4::orthographic(0.0, (double) WIDTH, (double) HEIGHT, 0.0, -1.0, 1.0);
    }

    inline float scaleX(int actualWidth)
    {
        return actualWidth > 0 ? (float) actualWidth / WIDTH : 1.0f;
    }

    inline float scaleY(int actualHeight)
    {
        return actualHeight > 0 ? (float) actualHeight / HEIGHT : 1.0f;
    }

    inline float scaleUniform(int actualWidth, int actualHeight)
    {
        return std::min(scaleX(actualWidth), scaleY(actualHeight));
    }

    inline float toActualX(float virtualX, int actualWidth)
    {
        return virtualX * scaleX(actualWidth);
    }

    inline float toActualY(float virtualY, int actualHeight)
    {
        return virtualY * scaleY(actualHeight);
    }

    inline float toActualLength(float virtualLength, int actualWidth, int actualHeight)
    {
        return virtualLength * scaleUniform(actualWidth, actualHeight);
    }

    inline Rect toActualRect(float virtualX, float virtualY, float virtualWidth,
                             float virtualHeight, int actualWidth, int actualHeight)
    {
        return {toActualX(virtualX, actualWidth), toActualY(virtualY, actualHeight),
                toActualLength(virtualWidth, actualWidth, actualHeight),
                toActualLength(virtualHeight, actualWidth, actualHeight)};
    }

    inline int toActualWidth(float virtualWidth, int actualWidth, int actualHeight)
    {
        int width = (int) lroundf(toActualLength(virtualWidth, actualWidth, actualHeight));
        return width > 0 ? width : 1;
    }

    inline int toActualHeight(float virtualHeight, int actualWidth, int actualHeight)
    {
        int height = (int) lroundf(toActualLength(virtualHeight, actualWidth, actualHeight));
        return height > 0 ? height : 1;
    }
} // namespace UIScreen
