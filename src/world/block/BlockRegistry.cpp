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
    s_grass.setTintColormap(Direction::UP, "foliage");

    static Block s_sand("sand", true, "textures/block/sand.png");
    static Block s_gravel("gravel", true, "textures/block/gravel.png");

    static Block s_glowstone("glowstone", true, "textures/block/glowstone.png");
    s_glowstone.setLightEmission(15);
    s_glowstone.setLightColor(0xFF, 0xDC, 0x85);

    static Block s_torch("torch", false, "textures/block/torch.png");
    s_torch.setRenderShape(Block::RenderShape::TORCH);
    s_torch.setAABB(AABB(Vec3(0.4375f, 0.0f, 0.4375f), Vec3(0.5625f, 0.625f, 0.5625f)));
    s_torch.setLightEmission(14);
    s_torch.setLightColor(0xFF, 0xE0, 0xB0);

    float sideU0 = 7.0f / 16.0f;
    float sideU1 = 9.0f / 16.0f;
    float sideV0 = 6.0f / 16.0f;
    float sideV1 = 16.0f / 16.0f;
    s_torch.setUVRect(Direction::NORTH, sideU0, sideV0, sideU1, sideV1);
    s_torch.setUVRect(Direction::SOUTH, sideU0, sideV0, sideU1, sideV1);
    s_torch.setUVRect(Direction::EAST, sideU0, sideV0, sideU1, sideV1);
    s_torch.setUVRect(Direction::WEST, sideU0, sideV0, sideU1, sideV1);
    s_torch.setUVRect(Direction::UP, 7.0f / 16.0f, 6.0f / 16.0f, 9.0f / 16.0f, 8.0f / 16.0f);

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
    registry->registerValue("torch", &s_torch);
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }
