#include "BlockRegistry.h"

#include "../../utils/Direction.h"

static TextureRepository s_textures;

BlockRegistry *BlockRegistry::get() {
    static BlockRegistry instance;
    return &instance;
}

void BlockRegistry::init() {
    BlockRegistry *registry = get();
    registry->registerValue("air", Block("air", false, ""));
    registry->registerValue("bedrock", Block("bedrock", true, "textures/block/bedrock.png"));
    registry->registerValue("stone", Block("stone", true, "textures/block/stone.png"));
    registry->registerValue("cobblestone", Block("cobblestone", true, "textures/block/cobblestone.png"));
    registry->registerValue("andesite", Block("andesite", true, "textures/block/andesite.png"));
    registry->registerValue("dirt", Block("dirt", true, "textures/block/dirt.png"));

    Block grass("grass", true, "textures/block/grass_side.png");
    grass.setTexture(Direction::UP, s_textures.get("textures/block/grass_top.png").get());
    grass.setTexture(Direction::DOWN, s_textures.get("textures/block/dirt.png").get());
    grass.setTintColormap(Direction::UP, "grasscolor");
    registry->registerValue("grass", grass);

    registry->registerValue("sand", Block("sand", true, "textures/block/sand.png"));
    registry->registerValue("gravel", Block("gravel", true, "textures/block/gravel.png"));

    Block glowstone("glowstone", true, "textures/block/glowstone.png");
    glowstone.setLightEmission(15);
    glowstone.setLightColor(0xFF, 0xDC, 0x85);
    registry->registerValue("glowstone", glowstone);
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }
