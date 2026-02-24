#include "BlockRegistry.h"

#include "../../utils/Direction.h"

static TextureRepository s_textures;

BlockRegistry *BlockRegistry::get() {
    static BlockRegistry instance;
    return &instance;
}

void BlockRegistry::init() {
    BlockRegistry *registry = get();

    static Block s_air("air", false, "");
    static Block s_bedrock("bedrock", true, "textures/block/bedrock.png");
    static Block s_stone("stone", true, "textures/block/stone.png");
    static Block s_cobblestone("cobblestone", true, "textures/block/cobblestone.png");
    static Block s_andesite("andesite", true, "textures/block/andesite.png");
    static Block s_dirt("dirt", true, "textures/block/dirt.png");

    static Block s_grass("grass", true, "textures/block/grass_side.png");
    s_grass.setTexture(Direction::UP, s_textures.get("textures/block/grass_top.png").get());
    s_grass.setTexture(Direction::DOWN, s_textures.get("textures/block/dirt.png").get());
    s_grass.setTintColormap(Direction::UP, "grasscolor");

    static Block s_sand("sand", true, "textures/block/sand.png");
    static Block s_gravel("gravel", true, "textures/block/gravel.png");

    static Block s_glowstone("glowstone", true, "textures/block/glowstone.png");
    s_glowstone.setLightEmission(15);
    s_glowstone.setLightColor(0xFF, 0xDC, 0x85);

    registry->registerValue("air", &s_air);
    registry->registerValue("bedrock", &s_bedrock);
    registry->registerValue("stone", &s_stone);
    registry->registerValue("cobblestone", &s_cobblestone);
    registry->registerValue("andesite", &s_andesite);
    registry->registerValue("dirt", &s_dirt);
    registry->registerValue("grass", &s_grass);
    registry->registerValue("sand", &s_sand);
    registry->registerValue("gravel", &s_gravel);
    registry->registerValue("glowstone", &s_glowstone);
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }
