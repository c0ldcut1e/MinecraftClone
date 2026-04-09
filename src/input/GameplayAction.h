#pragma once

#include <cstddef>

enum class GameplayAction : size_t
{
    Jump = 0,
    ToggleFly,
    BreakBlock,
    PlaceBlock,
    Count
};

constexpr size_t GAMEPLAY_ACTION_COUNT = (size_t) GameplayAction::Count;
