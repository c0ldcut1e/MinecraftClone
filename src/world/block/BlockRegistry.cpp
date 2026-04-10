#include "BlockRegistry.h"

#include <unordered_set>

#include "../../utils/Direction.h"

static TextureRepository s_textures;
static TextureAtlas s_blockAtlas;

static void setBlockTexture(Block &block, Direction *direction, const std::string &path)
{
    block.setTexture(direction, s_textures.get(path).get());
    block.setTexturePath(direction, path);
}

BlockRegistry *BlockRegistry::get()
{
    static BlockRegistry instance;
    return &instance;
}

void BlockRegistry::init()
{
    BlockRegistry *registry = get();

    static Block s_air("air", false, "");
    static Block s_worldBorder("world_border", true, "textures/block/bedrock.png");
    static Block s_bedrock("bedrock", true, "textures/block/bedrock.png");
    static Block s_stone("stone", true, "textures/block/stone.png");
    static Block s_cobblestone("cobblestone", true, "textures/block/cobblestone.png");
    static Block s_andesite("andesite", true, "textures/block/andesite.png");
    static Block s_dirt("dirt", true, "textures/block/dirt.png");

    static Block s_grass("grass", true, "textures/block/grass_side.png");
    setBlockTexture(s_grass, Direction::UP, "textures/block/grass_top.png");
    setBlockTexture(s_grass, Direction::DOWN, "textures/block/dirt.png");
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
    // TODO: add Direction::DOWN for UV rect

    static Block s_torchWall("torch_wall", false, "textures/block/torch.png");
    s_torchWall.setRenderShape(Block::RenderShape::TORCH);
    s_torchWall.setAABB(AABB(Vec3(0.4375f, 0.2f, 0.4375f), Vec3(0.5625f, 0.825f, 0.5625f)));
    s_torchWall.setInteractionAABB(AABB(Vec3(0.4f, 0.2f, 0.4f), Vec3(0.6f, 0.8f, 0.6f)));
    s_torchWall.setInteractionAttachmentOffset(-0.12f);
    s_torchWall.setLightEmission(14);
    s_torchWall.setLightColor(0xFF, 0xE0, 0xB0);
    s_torchWall.setUVRect(Direction::NORTH, sideU0, sideV0, sideU1, sideV1);
    s_torchWall.setUVRect(Direction::SOUTH, sideU0, sideV0, sideU1, sideV1);
    s_torchWall.setUVRect(Direction::EAST, sideU0, sideV0, sideU1, sideV1);
    s_torchWall.setUVRect(Direction::WEST, sideU0, sideV0, sideU1, sideV1);
    s_torchWall.setUVRect(Direction::UP, 7.0f / 16.0f, 6.0f / 16.0f, 9.0f / 16.0f, 8.0f / 16.0f);
    s_torchWall.setUVRect(Direction::DOWN, 0.0f, 0.0f, 0.0f, 0.0f);
    s_torchWall.setWallMountedTransform(22.5f, 0.475f);

    s_air.setSelectable(false);
    s_worldBorder.setSelectable(false);

    registry->registerValue("air", &s_air);
    registry->registerValue("world_border", &s_worldBorder);
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
    registry->registerValue("torch_wall", &s_torchWall);

    static Direction *directions[] = {Direction::UP,    Direction::DOWN, Direction::NORTH,
                                      Direction::SOUTH, Direction::EAST, Direction::WEST};
    std::vector<std::string> atlasPaths;
    std::unordered_set<std::string> seenPaths;

    for (uint32_t i = 0; i < registry->size(); i++)
    {
        Block *block = registry->byId(i);
        if (!block)
        {
            continue;
        }

        for (Direction *direction : directions)
        {
            const std::string &path = block->getTexturePath(direction);
            if (!path.empty() && seenPaths.insert(path).second)
            {
                atlasPaths.push_back(path);
            }
        }
    }

    s_blockAtlas.build(&s_textures, atlasPaths);
    Texture *atlasTexture = s_blockAtlas.getTexture();
    if (!atlasTexture)
    {
        return;
    }

    for (uint32_t i = 0; i < registry->size(); i++)
    {
        Block *block = registry->byId(i);
        if (!block)
        {
            continue;
        }

        for (Direction *direction : directions)
        {
            const std::string &path = block->getTexturePath(direction);
            if (path.empty() || !s_blockAtlas.has(path))
            {
                continue;
            }

            TextureAtlas::Region region = s_blockAtlas.get(path);
            block->setTexture(direction, atlasTexture);
            block->setAtlasUVRect(direction, region.u0, region.v0, region.u1, region.v1);
        }
    }
}

TextureRepository *BlockRegistry::getTextureRepository() { return &s_textures; }

TextureAtlas *BlockRegistry::getTextureAtlas() { return &s_blockAtlas; }
