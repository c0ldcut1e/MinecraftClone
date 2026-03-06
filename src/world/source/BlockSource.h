#pragma once

#include <cstdint>

#include "../block/BlockPos.h"

class BlockSource
{
public:
    virtual ~BlockSource() = default;

    virtual uint32_t getBlockId(const BlockPos &pos) const = 0;
};
