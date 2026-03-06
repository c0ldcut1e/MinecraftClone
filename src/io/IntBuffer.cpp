#include "IntBuffer.h"

#include <stdexcept>

#include "../memory/MemoryTracker.h"

IntBuffer IntBuffer::allocate(size_t capacity) { return IntBuffer(capacity); }

IntBuffer::IntBuffer() : m_data() {}

IntBuffer::IntBuffer(size_t capacity) : m_data()
{
    m_data = MemoryTracker::getInstance()->createIntBuffer(capacity);
    m_data.clear();
    m_data.reserve(capacity);
}

size_t IntBuffer::size() const { return m_data.size(); }

size_t IntBuffer::capacity() const { return m_data.capacity(); }

void IntBuffer::clear() { m_data.clear(); }

void IntBuffer::put(int32_t value)
{
    if (m_data.size() == m_data.capacity())
    {
        throw std::runtime_error("IntBuffer overflow");
    }

    m_data.push_back(value);
}

int32_t IntBuffer::get(size_t index) const
{
    if (index >= m_data.size())
    {
        throw std::runtime_error("IntBuffer index out of bounds");
    }

    return m_data[index];
}

const std::vector<int32_t> &IntBuffer::data() const { return m_data; }
