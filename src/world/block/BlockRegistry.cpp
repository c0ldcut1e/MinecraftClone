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
    registry->registerValue("bedrock", Block("bedrock", true, "textures/bedrock.png"));
    registry->registerValue("stone", Block("stone", true, "textures/stone.png"));
    registry->registerValue("cobblestone", Block("cobblestone", true, "textures/cobblestone.png"));
    registry->registerValue("andesite", Block("andesite", true, "textures/andesite.png"));
    registry->registerValue("dirt", Block("dirt", true, "textures/dirt.png"));

    Block grass("grass", true, "textures/grass_side.png");
    grass.setTexture(Direction::UP, s_textures.get("textures/grass_top.png").get());
    grass.setTexture(Direction::DOWN, s_textures.get("textures/dirt.png").get());
    registry->registerValue("grass", grass);
    registry->registerValue("sand", Block("sand", true, "textures/sand.png"));
    registry->registerValue("gravel", Block("gravel", true, "textures/gravel.png"));

    Block glowstone("glowstone", true, "textures/glowstone.png");
    glowstone.setLightEmission(15);
    glowstone.setLightColor(0xFF, 0xFF, 0xFF);
    registry->registerValue("glowstone", glowstone);
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }
