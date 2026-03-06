#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class IntBuffer
{
public:
    static IntBuffer allocate(size_t capacity);

    IntBuffer();

    size_t size() const;
    size_t capacity() const;

    void clear();

    void put(int32_t value);
    int32_t get(size_t index) const;

    const std::vector<int32_t> &data() const;

private:
    explicit IntBuffer(size_t capacity);

    std::vector<int32_t> m_data;
};
