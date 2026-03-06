#pragma once

#include <cstddef>
#include <cstdint>

class OutputStream
{
public:
    virtual ~OutputStream() = default;

    virtual void write(uint8_t value) = 0;

    virtual size_t write(const uint8_t *buffer, size_t count)
    {
        if (!buffer || count == 0)
        {
            return 0;
        }

        for (size_t i = 0; i < count; i++)
        {
            write(buffer[i]);
        }

        return count;
    }
};
