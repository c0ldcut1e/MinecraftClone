#pragma once

#include "BufferBuilder.h"

class Tesselator
{
public:
    static Tesselator *getInstance();

    BufferBuilder *getBuilderForScreen();
    BufferBuilder *getBuilderForLevel();

private:
    Tesselator();

    BufferBuilder m_screenBuilder;
    BufferBuilder m_levelBuilder;
};
