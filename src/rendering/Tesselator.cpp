#include "Tesselator.h"

Tesselator::Tesselator() : m_screenBuilder(0), m_levelBuilder(1) {}

Tesselator *Tesselator::getInstance()
{
    static Tesselator instance;
    return &instance;
}

BufferBuilder *Tesselator::getBuilderForScreen() { return &m_screenBuilder; }

BufferBuilder *Tesselator::getBuilderForLevel() { return &m_levelBuilder; }
