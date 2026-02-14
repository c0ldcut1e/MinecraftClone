#include "ChunkMesher.h"

#include <unordered_map>

#include "../../utils/Direction.h"
#include "../block/Block.h"
#include "../lighting/LightEngine.h"

static void push(std::vector<float> &vertices, float x, float y, float z, float u, float v, float r, float g, float b) {
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);

    vertices.push_back(u);
    vertices.push_back(v);

    vertices.push_back(r);
    vertices.push_back(g);
    vertices.push_back(b);
}

static void getFaceLight(World *world, const BlockPos &pos, Direction *direction, float *r, float *g, float *b) {
    BlockPos samplePos(pos.x + direction->dx, pos.y + direction->dy, pos.z + direction->dz);

    uint8_t lr;
    uint8_t lg;
    uint8_t lb;
    LightEngine::getLightLevel(world, samplePos, &lr, &lg, &lb);

    float minLight = 0.05f;
    float baseR    = (float) lr / 15.0f;
    float baseG    = (float) lg / 15.0f;
    float baseB    = (float) lb / 15.0f;

    baseR = baseR > minLight ? baseR : minLight;
    baseG = baseG > minLight ? baseG : minLight;
    baseB = baseB > minLight ? baseB : minLight;

    float shade = 1.0f;
    if (direction == Direction::NORTH || direction == Direction::SOUTH) shade = 0.8f;
    else if (direction == Direction::EAST || direction == Direction::WEST)
        shade = 0.6f;
    else if (direction == Direction::DOWN)
        shade = 0.5f;

    *r = baseR * shade;
    *g = baseG * shade;
    *b = baseB * shade;
}

static void addFace(std::vector<float> &vertices, World *world, Direction *direction, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, const BlockPos &pos) {
    float r;
    float g;
    float b;
    getFaceLight(world, pos, direction, &r, &g, &b);

    push(vertices, x1, y1, z1, 1, 1, r, g, b);
    push(vertices, x2, y2, z2, 1, 0, r, g, b);
    push(vertices, x3, y3, z3, 0, 0, r, g, b);

    push(vertices, x3, y3, z3, 0, 0, r, g, b);
    push(vertices, x4, y4, z4, 0, 1, r, g, b);
    push(vertices, x1, y1, z1, 1, 1, r, g, b);
}

static bool isSolid(World *world, const Chunk *chunk, int x, int y, int z) {
    if (y < 0 || y >= Chunk::SIZE_Y) return false;
    if (x >= 0 && x < Chunk::SIZE_X && z >= 0 && z < Chunk::SIZE_Z) return Block::byId(chunk->getBlockId(x, y, z))->isSolid();

    ChunkPos neighborPos = chunk->getPos();
    ChunkPos offset{0, 0, 0};

    if (x < 0) {
        offset += ChunkPos{-1, 0, 0};
        x += Chunk::SIZE_X;
    } else if (x >= Chunk::SIZE_X) {
        offset += ChunkPos{1, 0, 0};
        x -= Chunk::SIZE_X;
    }

    if (z < 0) {
        offset += ChunkPos{0, 0, -1};
        z += Chunk::SIZE_Z;
    } else if (z >= Chunk::SIZE_Z) {
        offset += ChunkPos{0, 0, 1};
        z -= Chunk::SIZE_Z;
    }

    neighborPos += offset;

    Chunk *neighbor = world->getChunk(neighborPos);
    if (!neighbor) return false;

    return Block::byId(neighbor->getBlockId(x, y, z))->isSolid();
}

void ChunkMesher::buildMeshes(World *world, const Chunk *chunk, std::vector<MeshBuildResult> &outMeshes) {
    std::shared_lock<std::shared_mutex> lock(world->getChunkDataMutex());

    std::unordered_map<Texture *, std::vector<float>> buckets;

    ChunkPos pos = chunk->getPos();

    for (int y = 0; y < Chunk::SIZE_Y; y++)
        for (int z = 0; z < Chunk::SIZE_Z; z++)
            for (int x = 0; x < Chunk::SIZE_X; x++) {
                Block *block = Block::byId(chunk->getBlockId(x, y, z));
                if (!block->isSolid()) continue;

                BlockPos blockPos(pos.x * Chunk::SIZE_X + x, pos.y * Chunk::SIZE_Y + y, pos.z * Chunk::SIZE_Z + z);

                Texture *texture;

                if (!isSolid(world, chunk, x, y, z - 1)) {
                    texture                      = block->getTexture(Direction::NORTH);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::NORTH, blockPos.x, blockPos.y, blockPos.z, blockPos.x, blockPos.y + 1, blockPos.z, blockPos.x + 1, blockPos.y + 1, blockPos.z, blockPos.x + 1, blockPos.y, blockPos.z, blockPos);
                }

                if (!isSolid(world, chunk, x, y, z + 1)) {
                    texture                      = block->getTexture(Direction::SOUTH);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::SOUTH, blockPos.x + 1, blockPos.y, blockPos.z + 1, blockPos.x + 1, blockPos.y + 1, blockPos.z + 1, blockPos.x, blockPos.y + 1, blockPos.z + 1, blockPos.x, blockPos.y, blockPos.z + 1, blockPos);
                }

                if (!isSolid(world, chunk, x, y + 1, z)) {
                    texture                      = block->getTexture(Direction::UP);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::UP, blockPos.x, blockPos.y + 1, blockPos.z, blockPos.x, blockPos.y + 1, blockPos.z + 1, blockPos.x + 1, blockPos.y + 1, blockPos.z + 1, blockPos.x + 1, blockPos.y + 1, blockPos.z, blockPos);
                }

                if (!isSolid(world, chunk, x, y - 1, z)) {
                    texture                      = block->getTexture(Direction::DOWN);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::DOWN, blockPos.x, blockPos.y, blockPos.z + 1, blockPos.x, blockPos.y, blockPos.z, blockPos.x + 1, blockPos.y, blockPos.z, blockPos.x + 1, blockPos.y, blockPos.z + 1, blockPos);
                }

                if (!isSolid(world, chunk, x + 1, y, z)) {
                    texture                      = block->getTexture(Direction::EAST);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::EAST, blockPos.x + 1, blockPos.y, blockPos.z, blockPos.x + 1, blockPos.y + 1, blockPos.z, blockPos.x + 1, blockPos.y + 1, blockPos.z + 1, blockPos.x + 1, blockPos.y, blockPos.z + 1, blockPos);
                }

                if (!isSolid(world, chunk, x - 1, y, z)) {
                    texture                      = block->getTexture(Direction::WEST);
                    std::vector<float> &vertices = buckets[texture];

                    addFace(vertices, world, Direction::WEST, blockPos.x, blockPos.y, blockPos.z + 1, blockPos.x, blockPos.y + 1, blockPos.z + 1, blockPos.x, blockPos.y + 1, blockPos.z, blockPos.x, blockPos.y, blockPos.z, blockPos);
                }
            }

    outMeshes.clear();

    for (std::unordered_map<Texture *, std::vector<float>>::iterator it = buckets.begin(); it != buckets.end(); ++it) {
        if (it->second.empty()) continue;

        MeshBuildResult result;
        result.texture  = it->first;
        result.vertices = it->second;

        outMeshes.push_back(result);
    }
}
