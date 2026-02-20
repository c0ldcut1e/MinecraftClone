#pragma once

#include <vector>

#include "../../rendering/Texture.h"
#include "../World.h"
#include "Chunk.h"

class ChunkMesher {
public:
    struct MeshBuildResult {
        Texture *texture;
        std::vector<float> vertices;
        std::vector<uint16_t> rawLights;
        std::vector<float> shades;
        std::vector<uint32_t> tints;
    };

    static void buildMeshes(World *world, const Chunk *chunk, std::vector<MeshBuildResult> &outMeshes);
};
