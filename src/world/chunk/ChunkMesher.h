#pragma once

#include <vector>

#include "../../graphics/Texture.h"
#include "../World.h"
#include "Chunk.h"

class ChunkMesher {
public:
    struct MeshBuildResult {
        Texture *texture;
        std::vector<float> vertices;
    };

    static void buildMeshes(World *world, const Chunk *chunk, std::vector<MeshBuildResult> &outMeshes);
};
