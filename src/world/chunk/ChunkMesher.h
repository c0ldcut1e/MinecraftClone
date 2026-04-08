#pragma once

#include <vector>

#include "../../rendering/Texture.h"
#include "../Level.h"
#include "Chunk.h"

class ChunkMesher
{
public:
    struct MeshBuildResult
    {
        Texture *texture;
        std::vector<float> vertices;
        std::vector<float> atlasRects;
        std::vector<uint16_t> rawLights;
        std::vector<float> shades;
        std::vector<uint32_t> tints;
    };

    static void buildMeshes(Level *level, const Chunk *chunk, bool smoothLighting,
                            bool grassSideOverlay, std::vector<MeshBuildResult> *outMeshes);
};
