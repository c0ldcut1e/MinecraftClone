#pragma once

#include <cstdint>

#include "Tag.h"

class ByteTag : public Tag
{
public:
    ByteTag();
    explicit ByteTag(int8_t value);

    Type getType() const override;

    int8_t getValue() const;
    void setValue(int8_t value);

private:
    int8_t m_value;
};
