#pragma once

#include "../../graphics/TextureRepository.h"
#include "../../utils/MappedRegistry.h"
#include "Block.h"

class BlockRegistry : public MappedRegistry<Block> {
public:
    static BlockRegistry *get();
    static void init();

    static TextureRepository *getTextureRepository();
};
