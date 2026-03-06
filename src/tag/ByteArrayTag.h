#pragma once

#include <cstdint>
#include <vector>

#include "Tag.h"

class ByteArrayTag : public Tag
{
public:
    ByteArrayTag();
    explicit ByteArrayTag(const std::vector<uint8_t> &value);

    Type getType() const override;

    const std::vector<uint8_t> &getValue() const;
    void setValue(const std::vector<uint8_t> &value);

private:
    std::vector<uint8_t> m_value;
};
