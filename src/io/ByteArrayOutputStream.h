#pragma once

#include <cstdint>
#include <vector>

#include "OutputStream.h"

class ByteArrayOutputStream : public OutputStream
{
public:
    ByteArrayOutputStream();

    void write(uint8_t value) override;

    const std::vector<uint8_t> &toByteArray() const;
    void clear();

private:
    std::vector<uint8_t> m_data;
};
