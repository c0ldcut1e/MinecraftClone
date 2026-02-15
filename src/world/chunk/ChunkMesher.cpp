#include "ChunkMesher.h"

#include <cmath>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "../../utils/Direction.h"
#include "../block/Block.h"
#include "../lighting/LightEngine.h"

struct MaskCell {
    Texture *texture;
    uint16_t lightKey;
    bool filled;
};

static inline uint16_t packLight(uint8_t r, uint8_t g, uint8_t b) {
    if (r > 15) r = 15;
    if (g > 15) g = 15;
    if (b > 15) b = 15;
    return (uint16_t) ((r << 8) | (g << 4) | (b));
}

static inline void unpackLight(uint16_t k, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (uint8_t) ((k >> 8) & 0xF);
    g = (uint8_t) ((k >> 4) & 0xF);
    b = (uint8_t) (k & 0xF);
}

static inline void push(std::vector<float> &vertices, float x, float y, float z, float u, float v, float r, float g, float b) {
    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(u);
    vertices.push_back(v);
    vertices.push_back(r);
    vertices.push_back(g);
    vertices.push_back(b);
}

static inline void shade(Direction *direction, uint8_t lr, uint8_t lg, uint8_t lb, float &r, float &g, float &b) {
    float minLight = 0.05f;
    float baseR    = (float) lr / 15.0f;
    float baseG    = (float) lg / 15.0f;
    float baseB    = (float) lb / 15.0f;

    if (baseR < minLight) baseR = minLight;
    if (baseG < minLight) baseG = minLight;
    if (baseB < minLight) baseB = minLight;

    float s = 1.0f;
    if (direction == Direction::NORTH || direction == Direction::SOUTH) s = 0.8f;
    else if (direction == Direction::EAST || direction == Direction::WEST)
        s = 0.6f;
    else if (direction == Direction::DOWN)
        s = 0.5f;

    r = baseR * s;
    g = baseG * s;
    b = baseB * s;
}

static inline void addFace(std::vector<float> &vertices, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, float uMax, float vMax, float r, float g, float b) {
    push(vertices, x1, y1, z1, uMax, vMax, r, g, b);
    push(vertices, x2, y2, z2, uMax, 0.0f, r, g, b);
    push(vertices, x3, y3, z3, 0.0f, 0.0f, r, g, b);

    push(vertices, x3, y3, z3, 0.0f, 0.0f, r, g, b);
    push(vertices, x4, y4, z4, 0.0f, vMax, r, g, b);
    push(vertices, x1, y1, z1, uMax, vMax, r, g, b);
}

static inline uint16_t sampleLightKey(World *world, int x, int y, int z) {
    uint8_t lr, lg, lb;
    LightEngine::getLightLevel(world, BlockPos(x, y, z), &lr, &lg, &lb);
    return packLight(lr, lg, lb);
}

static inline Block *getBlockWorld(World *world, const Chunk *chunk, int x, int y, int z) {
    if (y < 0 || y >= Chunk::SIZE_Y) return nullptr;
    if (x >= 0 && x < Chunk::SIZE_X && z >= 0 && z < Chunk::SIZE_Z) return Block::byId(chunk->getBlockId(x, y, z));

    ChunkPos pos = chunk->getPos();
    int lx       = x;
    int lz       = z;

    if (lx < 0) {
        pos.x -= 1;
        lx += Chunk::SIZE_X;
    } else if (lx >= Chunk::SIZE_X) {
        pos.x += 1;
        lx -= Chunk::SIZE_X;
    }

    if (lz < 0) {
        pos.z -= 1;
        lz += Chunk::SIZE_Z;
    } else if (lz >= Chunk::SIZE_Z) {
        pos.z += 1;
        lz -= Chunk::SIZE_Z;
    }

    Chunk *_chunk = world->getChunk(pos);
    if (!_chunk) return nullptr;

    return Block::byId(_chunk->getBlockId(lx, y, lz));
}

static inline bool isSolidWorld(World *world, const Chunk *chunk, int x, int y, int z) {
    if (y < 0 || y >= Chunk::SIZE_Y) return false;

    Block *block = getBlockWorld(world, chunk, x, y, z);
    if (!block) return true;
    return block->isSolid();
}

static void greedy2D(std::vector<MaskCell> &mask, int width, int height, auto &&emitRect) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            MaskCell cell = mask[(size_t) i + (size_t) width * (size_t) j];
            if (!cell.filled || !cell.texture) continue;

            int rWidth = 1;
            while (i + rWidth < width) {
                MaskCell _cell = mask[(size_t) (i + rWidth) + (size_t) width * (size_t) j];
                if (!_cell.filled || _cell.texture != cell.texture || _cell.lightKey != cell.lightKey) break;
                rWidth++;
            }

            int rHeight = 1;
            bool ok     = true;
            while (j + rHeight < height && ok) {
                for (int k = 0; k < rWidth; k++) {
                    MaskCell _cell = mask[(size_t) (i + k) + (size_t) width * (size_t) (j + rHeight)];
                    if (!_cell.filled || _cell.texture != cell.texture || _cell.lightKey != cell.lightKey) {
                        ok = false;
                        break;
                    }
                }

                if (ok) rHeight++;
            }

            emitRect(i, j, rWidth, rHeight, cell.texture, cell.lightKey);

            for (int y = 0; y < rHeight; y++)
                for (int x = 0; x < rWidth; x++) {
                    MaskCell &_cell = mask[(size_t) (i + x) + (size_t) width * (size_t) (j + y)];
                    _cell.filled    = false;
                    _cell.texture   = nullptr;
                    _cell.lightKey  = 0;
                }
        }
    }
}

void ChunkMesher::buildMeshes(World *world, const Chunk *chunk, std::vector<MeshBuildResult> &outMeshes) {
    std::shared_lock<std::shared_mutex> lock(world->getChunkDataMutex());

    std::unordered_map<Texture *, std::vector<float>> buckets;

    ChunkPos cpos = chunk->getPos();
    int baseX     = cpos.x * Chunk::SIZE_X;
    int baseY     = cpos.y * Chunk::SIZE_Y;
    int baseZ     = cpos.z * Chunk::SIZE_Z;

    auto emit = [&](Direction *direction, Texture *texture, uint16_t lightKey, float x1, float y1, float z1, float x2, float y2, float z2) {
        if (!texture) return;

        float uMax = 1.0f;
        float vMax = 1.0f;

        if (direction == Direction::NORTH || direction == Direction::SOUTH) {
            uMax = fabsf(x2 - x1);
            vMax = fabsf(y2 - y1);
        } else if (direction == Direction::EAST || direction == Direction::WEST) {
            uMax = fabsf(z2 - z1);
            vMax = fabsf(y2 - y1);
        } else {
            uMax = fabsf(x2 - x1);
            vMax = fabsf(z2 - z1);
        }

        uint8_t lr, lg, lb;
        unpackLight(lightKey, lr, lg, lb);

        float r, g, b;
        shade(direction, lr, lg, lb, r, g, b);

        std::vector<float> &v = buckets[texture];

        if (direction == Direction::NORTH) addFace(v, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1, uMax, vMax, r, g, b);
        else if (direction == Direction::SOUTH)
            addFace(v, x2, y1, z2, x2, y2, z2, x1, y2, z2, x1, y1, z2, uMax, vMax, r, g, b);
        else if (direction == Direction::UP)
            addFace(v, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1, uMax, vMax, r, g, b);
        else if (direction == Direction::DOWN)
            addFace(v, x1, y1, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, uMax, vMax, r, g, b);
        else if (direction == Direction::EAST)
            addFace(v, x2, y1, z1, x2, y2, z1, x2, y2, z2, x2, y1, z2, uMax, vMax, r, g, b);
        else if (direction == Direction::WEST)
            addFace(v, x1, y1, z2, x1, y2, z2, x1, y2, z1, x1, y1, z1, uMax, vMax, r, g, b);
    };

    {
        std::vector<MaskCell> mask;
        mask.resize((size_t) Chunk::SIZE_Z * (size_t) Chunk::SIZE_Y);

        for (int x = 0; x <= Chunk::SIZE_X; x++) {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
                for (int z = 0; z < Chunk::SIZE_Z; z++) {
                    bool a = isSolidWorld(world, chunk, x - 1, y, z);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) z + (size_t) Chunk::SIZE_Z * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (a && !b) {
                        Block *block     = getBlockWorld(world, chunk, x - 1, y, z);
                        Texture *texture = block ? block->getTexture(Direction::EAST) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x, baseY + y, baseZ + z);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_Z, Chunk::SIZE_Y, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float X  = (float) (baseX + x);
                float y1 = (float) (baseY + j);
                float y2 = (float) (baseY + (j + rHeight));
                float z1 = (float) (baseZ + i);
                float z2 = (float) (baseZ + (i + rWidth));
                emit(Direction::EAST, texture, lightKey, X, y1, z1, X, y2, z2);
            });
        }

        for (int x = 0; x <= Chunk::SIZE_X; x++) {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
                for (int z = 0; z < Chunk::SIZE_Z; z++) {
                    bool a = isSolidWorld(world, chunk, x - 1, y, z);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) z + (size_t) Chunk::SIZE_Z * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (b && !a) {
                        Block *block     = getBlockWorld(world, chunk, x, y, z);
                        Texture *texture = block ? block->getTexture(Direction::WEST) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x - 1, baseY + y, baseZ + z);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_Z, Chunk::SIZE_Y, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float X  = (float) (baseX + x);
                float y1 = (float) (baseY + j);
                float y2 = (float) (baseY + (j + rHeight));
                float z1 = (float) (baseZ + i);
                float z2 = (float) (baseZ + (i + rWidth));
                emit(Direction::WEST, texture, lightKey, X, y1, z1, X, y2, z2);
            });
        }
    }

    {
        std::vector<MaskCell> mask;
        mask.resize((size_t) Chunk::SIZE_X * (size_t) Chunk::SIZE_Y);

        for (int z = 0; z <= Chunk::SIZE_Z; z++) {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
                for (int x = 0; x < Chunk::SIZE_X; x++) {
                    bool a = isSolidWorld(world, chunk, x, y, z - 1);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (a && !b) {
                        Block *block     = getBlockWorld(world, chunk, x, y, z - 1);
                        Texture *texture = block ? block->getTexture(Direction::SOUTH) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x, baseY + y, baseZ + z);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Y, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float Z  = (float) (baseZ + z);
                float x1 = (float) (baseX + i);
                float x2 = (float) (baseX + (i + rWidth));
                float y1 = (float) (baseY + j);
                float y2 = (float) (baseY + (j + rHeight));
                emit(Direction::SOUTH, texture, lightKey, x1, y1, Z, x2, y2, Z);
            });
        }

        for (int z = 0; z <= Chunk::SIZE_Z; z++) {
            for (int y = 0; y < Chunk::SIZE_Y; y++)
                for (int x = 0; x < Chunk::SIZE_X; x++) {
                    bool a = isSolidWorld(world, chunk, x, y, z - 1);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) y];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (b && !a) {
                        Block *block     = getBlockWorld(world, chunk, x, y, z);
                        Texture *texture = block ? block->getTexture(Direction::NORTH) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x, baseY + y, baseZ + z - 1);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Y, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float Z  = (float) (baseZ + z);
                float x1 = (float) (baseX + i);
                float x2 = (float) (baseX + (i + rWidth));
                float y1 = (float) (baseY + j);
                float y2 = (float) (baseY + (j + rHeight));
                emit(Direction::NORTH, texture, lightKey, x1, y1, Z, x2, y2, Z);
            });
        }
    }

    {
        std::vector<MaskCell> mask;
        mask.resize((size_t) Chunk::SIZE_X * (size_t) Chunk::SIZE_Z);

        for (int y = 0; y <= Chunk::SIZE_Y; y++) {
            for (int z = 0; z < Chunk::SIZE_Z; z++)
                for (int x = 0; x < Chunk::SIZE_X; x++) {
                    bool a = isSolidWorld(world, chunk, x, y - 1, z);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) z];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (a && !b) {
                        Block *block     = getBlockWorld(world, chunk, x, y - 1, z);
                        Texture *texture = block ? block->getTexture(Direction::UP) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x, baseY + y, baseZ + z);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Z, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float Y  = (float) (baseY + y);
                float x1 = (float) (baseX + i);
                float x2 = (float) (baseX + (i + rWidth));
                float z1 = (float) (baseZ + j);
                float z2 = (float) (baseZ + (j + rHeight));
                emit(Direction::UP, texture, lightKey, x1, Y, z1, x2, Y, z2);
            });
        }

        for (int y = 0; y <= Chunk::SIZE_Y; y++) {
            for (int z = 0; z < Chunk::SIZE_Z; z++)
                for (int x = 0; x < Chunk::SIZE_X; x++) {
                    bool a = isSolidWorld(world, chunk, x, y - 1, z);
                    bool b = isSolidWorld(world, chunk, x, y, z);

                    MaskCell &cell = mask[(size_t) x + (size_t) Chunk::SIZE_X * (size_t) z];
                    cell.filled    = false;
                    cell.texture   = nullptr;
                    cell.lightKey  = 0;

                    if (b && !a) {
                        Block *block     = getBlockWorld(world, chunk, x, y, z);
                        Texture *texture = block ? block->getTexture(Direction::DOWN) : nullptr;
                        if (texture) {
                            cell.filled   = true;
                            cell.texture  = texture;
                            cell.lightKey = sampleLightKey(world, baseX + x, baseY + y - 1, baseZ + z);
                        }
                    }
                }

            greedy2D(mask, Chunk::SIZE_X, Chunk::SIZE_Z, [&](int i, int j, int rWidth, int rHeight, Texture *texture, uint16_t lightKey) {
                float Y  = (float) (baseY + y);
                float x1 = (float) (baseX + i);
                float x2 = (float) (baseX + (i + rWidth));
                float z1 = (float) (baseZ + j);
                float z2 = (float) (baseZ + (j + rHeight));
                emit(Direction::DOWN, texture, lightKey, x1, Y, z1, x2, Y, z2);
            });
        }
    }

    outMeshes.clear();
    outMeshes.reserve(buckets.size());

    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
        if (!it->first) continue;
        if (it->second.empty()) continue;

        MeshBuildResult result;
        result.texture  = it->first;
        result.vertices = std::move(it->second);
        outMeshes.push_back(std::move(result));
    }
}
