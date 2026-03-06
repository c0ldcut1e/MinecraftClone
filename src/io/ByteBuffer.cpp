#include "ByteBuffer.h"

#include <stdexcept>

#include "../memory/MemoryTracker.h"

ByteBuffer ByteBuffer::allocate(size_t capacity) { return ByteBuffer(capacity); }

ByteBuffer::ByteBuffer() : m_data(), m_position(0), m_limit(0) {}

ByteBuffer::ByteBuffer(size_t capacity) : m_data(), m_position(0), m_limit(capacity)
{
    m_data = MemoryTracker::getInstance()->createByteBuffer(capacity);
    m_data.clear();
    m_data.reserve(capacity);
}

size_t ByteBuffer::size() const { return m_data.size(); }

size_t ByteBuffer::capacity() const { return m_data.capacity(); }

size_t ByteBuffer::position() const { return m_position; }

size_t ByteBuffer::remaining() const { return m_limit > m_position ? m_limit - m_position : 0; }

void ByteBuffer::clear()
{
    m_data.clear();
    m_position = 0;
    m_limit    = m_data.capacity();
}

void ByteBuffer::flip()
{
    m_limit    = m_data.size();
    m_position = 0;
}

void ByteBuffer::put(uint8_t value)
{
    if (m_data.size() >= m_limit)
    {
        throw std::runtime_error("ByteBuffer overflow");
    }

    m_data.push_back(value);
}

void ByteBuffer::put(size_t index, uint8_t value)
{
    if (index >= m_data.size())
    {
        throw std::runtime_error("ByteBuffer index out of bounds");
    }

    m_data[index] = value;
}

uint8_t ByteBuffer::get()
{
    if (m_position >= m_limit || m_position >= m_data.size())
    {
        throw std::runtime_error("ByteBuffer underflow");
    }

    return m_data[m_position++];
}

uint8_t ByteBuffer::get(size_t index) const
{
    if (index >= m_data.size())
    {
        throw std::runtime_error("ByteBuffer index out of bounds");
    }

    return m_data[index];
}

const std::vector<uint8_t> &ByteBuffer::data() const { return m_data; }
