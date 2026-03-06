#pragma once

#include <cstdint>

class Tag
{
public:
    enum Type : uint8_t
    {
        BYTE       = 1,
        BYTE_ARRAY = 7
    };

    virtual ~Tag() = default;

    virtual Type getType() const = 0;
};
