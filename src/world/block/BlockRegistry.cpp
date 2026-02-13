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
    registry->registerValue("stone", Block("stone", true, "textures/stone.png"));
    registry->registerValue("bedrock", Block("bedrock", true, "textures/bedrock.png"));
    registry->registerValue("dirt", Block("dirt", true, "textures/dirt.png"));

    Block grass("grass", true, "textures/grass_side.png");
    grass.setTexture(Direction::UP, s_textures.get("textures/grass_top.png").get());
    grass.setTexture(Direction::DOWN, s_textures.get("textures/dirt.png").get());
    registry->registerValue("grass", grass);

    Block glowstone("glowstone", true, "textures/glowstone.png");
    glowstone.setLightEmission(20);
    glowstone.setLightColor(0xE3, 0x9F, 0x00);
    registry->registerValue("glowstone", glowstone);

    Block light("light", false, "");
    light.setLightEmission(15);
    light.setLightColor(0xFF, 0xEE, 0xFF);
    registry->registerValue("light", light);
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }
