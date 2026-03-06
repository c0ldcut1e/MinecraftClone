#include "ByteArrayInputStream.h"

#include <algorithm>

ByteArrayInputStream::ByteArrayInputStream() : m_data(), m_position(0) {}

ByteArrayInputStream::ByteArrayInputStream(const std::vector<uint8_t> &data)
    : m_data(data), m_position(0)
{}

int ByteArrayInputStream::read()
{
    if (m_position >= m_data.size())
    {
        return -1;
    }

    return (int) m_data[m_position++];
}

size_t ByteArrayInputStream::read(uint8_t *buffer, size_t count)
{
    if (!buffer || count == 0)
    {
        return 0;
    }
    if (m_position >= m_data.size())
    {
        return 0;
    }

    size_t remaining = m_data.size() - m_position;
    size_t toRead    = std::min(count, remaining);

    for (size_t i = 0; i < toRead; i++)
    {
        buffer[i] = m_data[m_position + i];
    }

    m_position += toRead;
    return toRead;
}

size_t ByteArrayInputStream::available() const
{
    if (m_position >= m_data.size())
    {
        return 0;
    }

    return m_data.size() - m_position;
}

void ByteArrayInputStream::reset() { m_position = 0; }
