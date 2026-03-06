#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class ByteBuffer
{
public:
    static ByteBuffer allocate(size_t capacity);

    ByteBuffer();

    size_t size() const;
    size_t capacity() const;
    size_t position() const;
    size_t remaining() const;

    void clear();
    void flip();

    void put(uint8_t value);
    void put(size_t index, uint8_t value);

    uint8_t get();
    uint8_t get(size_t index) const;

    const std::vector<uint8_t> &data() const;

private:
    explicit ByteBuffer(size_t capacity);

    std::vector<uint8_t> m_data;
    size_t m_position;
    size_t m_limit;
};
