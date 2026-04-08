#pragma once

#include <cstdint>

#include "BlockPos.h"

class BlockSource
{
public:
    virtual ~BlockSource() = default;

    virtual uint32_t getBlockId(const BlockPos &pos) const = 0;
};
